import SwiftUI

@main
struct InsertCoinApp: App {
    #if os(iOS)
    @UIApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    #elseif os(macOS)
    @NSApplicationDelegateAdaptor(MacAppDelegate.self) private var appDelegate
    #endif

    init() {
        SetRenames.migrate()
        #if os(macOS)
        // --video: make gameplay videos with no window, then quit (VideoJob).
        if let job = VideoJob(arguments: CommandLine.arguments) { exit(job.run()) }
        #endif
        SharedSettings.start()
    }

    var body: some Scene {
        #if os(macOS)
        // One window on the Mac: no New Window and no tabs (MacAppDelegate).
        Window("Arcade Foundry: Insert Coin", id: "library") {
            RootView()
                .appAppearance()
                .frame(minWidth: 640, minHeight: 480)
        }
        Settings {
            AppSettingsView().frame(width: 460).appAppearance()
        }
        // The app menu's About item opens the logo, version and credits.
        .commands {
            CommandGroup(replacing: .appInfo) { AboutCommand() }
            CommandGroup(replacing: .help) { HelpCommand() }
        }
        Window("About Arcade Foundry: Insert Coin", id: AboutView.windowID) {
            AboutView().appAppearance()
        }
        .windowResizability(.contentSize)
        // The game in an Apple Vision Pro, rendered here (HeadsetSpace).
        RemoteImmersiveSpace(id: StereoBridge.spaceID) {
            HeadsetSpace()
        }
        .immersionStyle(selection: .constant(.full), in: .full)
        #else
        WindowGroup {
            RootView().appAppearance()
        }
        #endif
    }
}

/// The library, or the game being played.
struct RootView: View {
    @State private var playing: GameInfo?
    /// The game pages open over the library, kept while a game plays.
    @State private var libraryPath: [String] = []
    @State private var session: GameSession?
    @State private var error: String?
    @State private var preparing: String?
    @State private var continueAs: CabinetList?
    @State private var showSplash = true
    @AppStorage(SettingKey.volume) private var volume = 0.8
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        Group {
            if let session {
                PlayView(session: session) { stop() }
            } else {
                NavigationStack(path: $libraryPath) {
                    LibraryView(playing: $playing, path: $libraryPath)
                }
                .overlay {
                    if let preparing {
                        ProgressView(preparing)
                            .padding(24)
                            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                    }
                }
            }
        }
        .overlay {
            if showSplash {
                SplashView()
                    .transition(.opacity)
                    .onTapGesture { dismissSplash() }
            }
        }
        .onAppear {
            CloudROMs.shared.start()
            LinkDiscovery.shared.start()
            SaveSync.start()
        }
        .task {
            try? await Task.sleep(nanoseconds: 1_000_000_000) // was 1.8 s (#193)
            dismissSplash()
        }
        .task { await meetCabinets() }
        .task { await launchArguments() }
        .sheet(item: $continueAs) { list in
            ContinueAsView(cabinets: list.cabinets) { continueAs = nil }.appAppearance()
        }
        .onChange(of: playing) { game in
            GameArt.shared.holding = game != nil // no art downloads while a game plays
            if let game { start(game) }
        }
        .onChange(of: volume) { v in session?.setVolume(v) }
        .onChange(of: scenePhase) { phase in
            if phase == .active { Task { await SaveSync.retryPending() } }
        }
        #if os(iOS)
        .onChange(of: session == nil) { idle in AppDelegate.game(!idle) }
        #endif
        .alert("Could not start the game", isPresented: Binding(get: { error != nil }, set: { if !$0 { error = nil } })) {
            Button("OK") { error = nil }
        } message: {
            Text(error ?? "")
        }
    }

    private func dismissSplash() {
        withAnimation(.easeOut(duration: 0.4)) { showSplash = false }
        LaunchTimer.splashGone()
    }

    private func start(_ game: GameInfo) {
        let resume = SaveStates.resumeNext == game.id // the game's page chose Resume
        SaveStates.resumeNext = nil
        if PCGames.isPC(game.id) { // its runner, in a process of its own, with its own window (PCGames)
            Task { @MainActor in
                do {
                    try await PCGames.play(game)
                } catch {
                    self.error = error.localizedDescription
                }
                playing = nil
            }
            return
        }
        guard let lib = CoreLibrary.load(game.id) else {
            print("play: \(game.id) has no core in this build")
            playing = nil
            return
        }
        Task { @MainActor in
            do {
                if Storage.rom(game.id) == nil {
                    preparing = Storage.isFolder(game.id) ? "Downloading \(game.title) from iCloud Drive and unpacking it…"
                                                          : "Downloading \(game.title) from iCloud Drive…"
                }
                let rom = try await Storage.ready(game.id)
                preparing = "Starting \(game.title)…"
                await SaveSync.sync(game.id) // the newest of this cabinet's saves, from any device sharing it
                preparing = nil
                guard playing == game, session == nil else { return }
                session = try GameSession(game: game, library: lib, rom: rom, volume: volume, resume: resume)
                print("play: \(game.id) started")
                LastPlayed.mark(game.id)
                StereoBridge.shared.play(game)
            } catch {
                print("play: \(game.id) did not start: \(error.localizedDescription)")
                preparing = nil
                self.error = error.localizedDescription
                playing = nil
            }
        }
    }

    /// On a device new to Arcade Foundry: Insert Coin (or one that lost its cabinet ID),
    /// offers to continue as one of the cabinets in iCloud; otherwise
    /// records this cabinet there.
    private func meetCabinets() async {
        _ = Cabinet.id
        if Cabinet.isNew, await SaveSync.available(),
           let list = try? await SaveSync.cabinets().filter({ $0.id != Cabinet.id }), !list.isEmpty {
            continueAs = CabinetList(cabinets: list)
        } else {
            await SaveSync.announce()
        }
    }

    /// `--play <set> [--seconds N]`: start that game as a tap in the library
    /// would, and quit after N seconds. For unattended device test runs
    /// (devicectl launches the app with arguments and attaches the console).
    private func launchArguments() async {
        let args = CommandLine.arguments
        guard let i = args.firstIndex(of: "--play"), i + 1 < args.count,
              let game = GameInfo.all.first(where: { $0.id == args[i + 1] }) else { return }
        try? await Task.sleep(nanoseconds: 1_500_000_000) // past the splash
        print("launch: --play \(game.id)")
        playing = game
        guard let j = args.firstIndex(of: "--seconds"), j + 1 < args.count,
              let seconds = UInt64(args[j + 1]), seconds > 0 else { return }
        try? await Task.sleep(nanoseconds: seconds * 1_000_000_000)
        print("launch: --seconds \(seconds) reached, quitting")
        stop()
        exit(0)
    }

    private func stop() {
        session?.stop()
        StereoBridge.shared.play(nil)
        session = nil
        playing = nil
    }
}

/// App settings: sound, picture, iCloud Drive and the ROM server.
struct AppSettingsView: View {
    @AppStorage(SettingKey.romServer) private var romServer = ""
    @AppStorage(SettingKey.addToCloud) private var addToCloud = true
    @ObservedObject private var cloud = CloudROMs.shared
    @State private var moving = false
    @State private var moveResult: String?
    @AppStorage(SettingKey.volume) private var volume = 0.8
    @AppStorage(SettingKey.screenLook) private var look = ScreenLook.standard
    @AppStorage(SettingKey.coreThread) private var coreThread = true
    @AppStorage(SettingKey.recordReplay) private var recordReplays = false
    @AppStorage(SettingKey.showSpeed) private var showSpeed = false
    @AppStorage(SettingKey.resumeGames) private var resumeGames = false
    #if os(macOS)
    private static let speedNote = " (the window title always has it; this is for full screen)"
    #else
    private static let speedNote = ""
    #endif
    @AppStorage(SettingKey.appearance) private var appearance = Appearance.dark
    @AppStorage(SettingKey.downloadArt) private var downloadArt = true
    @AppStorage(SettingKey.touchControls) private var touchControls = TouchControls.automatic
    @State private var artSize = GameArt.size()
    @State private var showCabinets = false

    var body: some View {
        Form {
            Section("Sound") {
                #if os(tvOS)
                Picker("Volume", selection: $volume) {
                    ForEach([0.0, 0.2, 0.4, 0.6, 0.8, 1.0], id: \.self) { Text("\(Int($0 * 100))%").tag($0) }
                }
                #else
                Slider(value: $volume, in: 0...1) { Text("Volume") }
                #endif
            }
            Section("Appearance") {
                Picker("Appearance", selection: $appearance) {
                    ForEach(Appearance.allCases) { Text($0.name).tag($0) }
                }
            }
            Section("Screen") {
                ScreenLookControls(look: $look)
                Text("For every game; a game's page can give it its own.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            #if !os(tvOS)
            Section("Replays") {
                Toggle("Record replays", isOn: $recordReplays)
                Text("Keeps your controls from the start of each game and saves them as a replay in the Replays folder (iCloud Drive) when you leave it. The Mac turns a replay into a video of the game exactly as you played it. For every game; a game's page or the pause menu can give it its own.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            #endif
            Section("Save states") {
                Toggle("Resume where I left off", isOn: $resumeGames)
                Text("When you leave a game, it is kept as it was, and its page offers Resume beside Play. Model 3 games for now; not with link play. Save state and Load state in the pause menu work either way.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Section("Speed") {
                Toggle("Game on its own thread", isOn: $coreThread)
                Text("Try it on a device where a game runs slow: the screen then keeps its own rate. This device only; also in the pause menu.")
                    .font(.caption).foregroundStyle(.secondary)
                Toggle("Show speed", isOn: $showSpeed)
                Text("Shows the frame rate and the game's speed in a corner while a game runs\(Self.speedNote).")
                    .font(.caption).foregroundStyle(.secondary)
                if let last = UserDefaults.standard.string(forKey: LaunchTimer.key) {
                    LabeledContent("Last start", value: last)
                }
            }
            #if os(iOS)
            Section("On-screen controller") {
                Picker("On-screen controller", selection: $touchControls) {
                    ForEach(TouchControls.allCases) { Text($0.name).tag($0) }
                }
                Text("The controller drawn over a game on the touch screen. Automatic shows it while no controller is connected, and hides it while one is. Always shows it beside a controller too, as player 2. The gun games aim by touch unless their page says otherwise. Coin, Start and Pause are always there. A game's page can give it its own.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            #endif
            Section("Game art") {
                Toggle("Download game art", isOn: $downloadArt)
                Text("Title screens, flyers, marquees and cabinets for the library and each game's page, downloaded to this device as you browse, from the Arcade Database (adb.arcadeitalia.net) and libretro-thumbnails. Only the game's MAME set name is sent. Your own pictures always come first.")
                    .font(.caption).foregroundStyle(.secondary)
                Button("Remove downloaded art (\(ByteCountFormatter.string(fromByteCount: artSize, countStyle: .file)))", role: .destructive) {
                    GameArt.shared.removeAll()
                    artSize = GameArt.size()
                }
                .disabled(artSize == 0)
                Text("Your own pictures stay. With Download game art on, the art downloads again as you browse.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            CabinetSettingsSection(showCabinets: $showCabinets)
            #if os(macOS)
            WheelSettingsSection()
            #endif
            #if !os(tvOS)
            Section("iCloud Drive") {
                if let folder = cloud.folder {
                    Toggle("Add ROM puts ROM sets in iCloud Drive", isOn: $addToCloud)
                    Button(moving ? "Putting them in iCloud Drive…" : "Put this device's ROM sets in iCloud Drive") { moveAll(to: folder) }
                        .disabled(moving || GameInfo.all.allSatisfy { Storage.rom($0.id) == nil })
                    if let moveResult {
                        Text(moveResult).font(.caption)
                    }
                    Text("ROM sets in iCloud Drive > Arcade Foundry are shared by your Macs, iPhones and iPads, and download when you play. You can also put them in that folder from the Finder or Files (named after the MAME set: srallyc.zip). A set put in iCloud Drive stays on this device (the app keeps no second copy) until the system needs the space; it then downloads again when you play.")
                        .font(.caption).foregroundStyle(.secondary)
                } else {
                    Text("iCloud Drive is off for Arcade Foundry: Insert Coin: sign in to iCloud and turn on iCloud Drive to share your ROM sets between your devices.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
            #endif
            Section("About") {
                Image("Logo")
                    .resizable()
                    .scaledToFit()
                    .frame(maxWidth: .infinity, maxHeight: 140)
                    .padding(.vertical, 12)
                    .listRowBackground(Color("LaunchBackground"))
                LabeledContent("Arcade Foundry: Insert Coin", value: AppInfo.version)
                LabeledContent("Core interface", value: CoreLibrary.interfaceVersion)
                #if os(macOS)
                AboutCreditsButton()
                #else
                NavigationLink("Credits") { CreditsView() }
                #endif
            }
            Section("ROM server") {
                TextField("URL", text: $romServer, prompt: Text("http://192.168.1.10:8000/roms"))
                Text("Games download <URL>/<romset>.zip (or .7z) from your own server, once per device. The address is shared with your other devices through iCloud. Your ROM sets stay yours: nothing is uploaded anywhere." + iCloudNote)
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
        .sheet(isPresented: $showCabinets) { CabinetsView().appAppearance() }
        .onChange(of: romServer) { SharedSettings.push(SettingKey.romServer, $0) }
        .formStyle(.grouped)
        #if os(macOS)
        .padding()
        #endif
    }

    private var iCloudNote: String {
        #if os(tvOS)
        " Apple TV has no iCloud Drive: it downloads every game it is missing from here."
        #else
        ""
        #endif
    }

    private func moveAll(to folder: URL) {
        moving = true
        moveResult = nil
        Task {
            var moved: [String] = [], failed: [String] = []
            for game in GameInfo.all where Storage.rom(game.id) != nil {
                do {
                    try await Storage.moveToCloud(game.id, folder: folder)
                    moved.append(game.id)
                } catch {
                    failed.append("\(game.id) (\(error.localizedDescription))")
                }
            }
            moving = false
            moveResult = (moved.isEmpty ? "" : "In iCloud Drive now: \(moved.joined(separator: ", ")). ") + (failed.isEmpty ? "" : "Not put in iCloud Drive: \(failed.joined(separator: ", ")).")
        }
    }
}

/// How long the app took to start (#202): from the process starting to the
/// library's first appearance, and to the library on screen once the splash
/// goes. Printed, and kept for Settings (Speed). Launched from Xcode the
/// debugger adds to both: start from the Home Screen to measure.
enum LaunchTimer {
    static let key = "lastLaunch"
    private static var ready: Double?
    private static var splash: Double?
    private static var done = false

    /// Seconds since this process started (its start time from the kernel).
    private static func elapsed() -> Double {
        var info = kinfo_proc()
        var size = MemoryLayout<kinfo_proc>.stride
        var mib: [Int32] = [CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()]
        guard sysctl(&mib, u_int(mib.count), &info, &size, nil, 0) == 0 else { return 0 }
        let start = info.kp_proc.p_un.__p_starttime
        return Date().timeIntervalSince1970 - (Double(start.tv_sec) + Double(start.tv_usec) / 1_000_000)
    }

    static func libraryShown() {
        guard ready == nil else { return }
        ready = elapsed()
        record()
    }

    static func splashGone() {
        guard splash == nil else { return }
        splash = elapsed()
        record()
    }

    /// Once both are in: the library is on screen when the later of the two happened.
    private static func record() {
        guard !done, let ready, let splash else { return }
        done = true
        let text = String(format: "library ready in %.1f s, on screen in %.1f s", ready, max(ready, splash))
        print("launch: \(text)")
        UserDefaults.standard.set(text, forKey: key)
    }
}
