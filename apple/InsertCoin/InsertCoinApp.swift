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

#if os(macOS)
/// The Mac's menu bar has no File or Edit menu, and the window no tabs.
final class MacAppDelegate: NSObject, NSApplicationDelegate {
    private var keys: Any?

    func applicationWillFinishLaunching(_ notification: Notification) {
        NSWindow.allowsAutomaticWindowTabbing = false // and View's tab items
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        // The Edit menu's items carried the text keys; text fields keep them.
        keys = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { event in
            let mods = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
            let action: String? = switch (mods, event.charactersIgnoringModifiers?.lowercased()) {
            case (.command, "x"): "cut:"
            case (.command, "c"): "copy:"
            case (.command, "v"): "paste:"
            case (.command, "a"): "selectAll:"
            case (.command, "z"): "undo:"
            case ([.command, .shift], "z"): "redo:"
            default: nil
            }
            guard let action, NSApp.sendAction(Selector(action), to: nil, from: nil) else { return event }
            return nil
        }
    }

    /// SwiftUI builds the menu bar again from time to time; take the two
    /// menus out each time (found by their items, so in any language).
    func applicationWillUpdate(_ notification: Notification) {
        guard let bar = NSApp.mainMenu else { return }
        let unwanted = [#selector(NSWindow.performClose(_:)), Selector(("undo:")), #selector(NSText.copy(_:))]
        for item in bar.items.dropFirst() {
            let actions = item.submenu?.items.compactMap(\.action) ?? []
            if actions.contains(where: unwanted.contains), !actions.contains(#selector(NSWindow.performMiniaturize(_:))) {
                bar.removeItem(item) // File (Close) or Edit, never Window
            }
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}
#endif

#if os(iOS)
/// iPhone: the library turns upright too, a game is landscape only (the app
/// turns to landscape while one is up). iPad stays landscape.
final class AppDelegate: NSObject, UIApplicationDelegate {
    private static var gameUp = false

    func application(_ application: UIApplication, supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        Self.gameUp || UIDevice.current.userInterfaceIdiom == .pad ? .landscape : .allButUpsideDown
    }

    /// Called as a game starts and stops.
    @MainActor static func game(_ up: Bool) {
        gameUp = up
        for case let scene as UIWindowScene in UIApplication.shared.connectedScenes {
            scene.keyWindow?.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
            if up { scene.requestGeometryUpdate(.iOS(interfaceOrientations: .landscape)) }
        }
    }
}
#endif

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
        .sheet(item: $continueAs) { list in
            ContinueAsView(cabinets: list.cabinets) { continueAs = nil }.appAppearance()
        }
        .onChange(of: playing) { game in
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
        guard let lib = CoreLibrary.load(game.id) else {
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
                session = try GameSession(game: game, library: lib, rom: rom, volume: volume)
                LastPlayed.mark(game.id)
                StereoBridge.shared.play(game)
            } catch {
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

    private func stop() {
        session?.stop()
        StereoBridge.shared.play(nil)
        session = nil
        playing = nil
    }
}

private extension View {
    /// The game fills the screen on iOS and tvOS. On macOS it stays below
    /// the window's title bar, which macOS draws over the content.
    @ViewBuilder func underTitleBar() -> some View {
        #if os(macOS)
        self
        #else
        ignoresSafeArea()
        #endif
    }
}

/// The game full window, with the link status over it when linked, and
/// the pause menu over it when open.
struct PlayView: View {
    @ObservedObject var session: GameSession
    var onExit: () -> Void
    @Environment(\.scenePhase) private var phase
    @AppStorage(SettingKey.screenLook) private var globalLook = ScreenLook.standard.rawValue
    @AppStorage private var gameLook: String
    @AppStorage private var crosshair: Bool

    init(session: GameSession, onExit: @escaping () -> Void) {
        self.session = session
        self.onExit = onExit
        _gameLook = AppStorage(wrappedValue: "", ScreenLook.gameKey(session.game.id))
        _crosshair = AppStorage(wrappedValue: true, SettingKey.crosshair(session.game.id))
    }

    private var look: ScreenLook { ScreenLook.resolved(global: globalLook, game: gameLook) }

    var body: some View {
        ZStack(alignment: .bottomLeading) {
            Color.black.ignoresSafeArea()
            #if os(tvOS)
            GameView(session: session, look: look, capture: session.stopped == nil) // the pause menu polls the controller too
                .ignoresSafeArea()
            #else
            GameView(session: session, look: look).underTitleBar()
            #endif
            if session.game.showsCrosshair && crosshair {
                CrosshairView(shown: session.controls.shown).underTitleBar()
            }
            if session.operatorMode {
                VStack {
                    Text(operatorHint)
                        .font(.caption.weight(.semibold)).padding(8)
                        .background(.orange.opacity(0.85), in: RoundedRectangle(cornerRadius: 6))
                        .foregroundStyle(.black).padding(.top, 60)
                    Spacer()
                }
                .frame(maxWidth: .infinity)
            }
            if session.recording {
                VStack {
                    HStack {
                        Spacer()
                        Circle().fill(.red).frame(width: 12, height: 12)
                            .accessibilityLabel("Recording a replay")
                    }
                    Spacer()
                }
                .padding(.top, 70).padding(.trailing, 24)
                .allowsHitTesting(false)
            }
            if !session.status.isEmpty {
                Text(session.status)
                    .font(.caption).padding(6)
                    .background(.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 6))
                    .foregroundStyle(.white).padding(12)
            }
            #if os(iOS)
            VStack {
                HStack(spacing: 12) {
                    Button { session.openMenu() } label: { Image(systemName: "pause.fill") }
                        .buttonStyle(.bordered).tint(.white)
                    SpeedLabel(speed: session.speed)
                    Spacer()
                    if session.operatorMode {
                        TouchButton(title: "Test", bit: UInt32(AFCORE_TEST), controls: session.controls)
                        TouchButton(title: "Service", bit: UInt32(AFCORE_SERVICE), controls: session.controls)
                    }
                    TouchButton(title: "Coin", bit: UInt32(AFCORE_COIN), controls: session.controls)
                    TouchButton(title: "Start", bit: UInt32(AFCORE_START), controls: session.controls)
                }
                .padding(16)
                Spacer()
            }
            #endif
            #if os(tvOS)
            VStack {
                HStack { Spacer(); SpeedLabel(speed: session.speed) }
                Spacer()
            }
            .padding(40)
            #endif
        }
        #if os(macOS)
        .navigationTitle(session.speed.isEmpty ? session.game.title : "\(session.game.title) — \(session.speed)")
        .toolbar { if ThisRelease.headsetButton { ToolbarItem { HeadsetButton() } } }
        .focusedSceneValue(\.gameSession, session) // Help shows its controls
        #endif
        #if os(iOS)
        .persistentSystemOverlays(.hidden)
        .onAppear { session.controls.showTouchControls(!["vcop2", "vcop", "hotdo", "gunblade", "bel", "rchase2", "hotd4a", "letsgoju"].contains(session.game.id) && !session.game.analogGun, fighting: session.game.fighting, fishing: session.game.fishing) } // the gun is the touch screen
        .onDisappear { session.controls.showTouchControls(false) }
        #endif
        .overlay {
            if session.menuOpen { PauseMenu(session: session) }
        }
        .onChange(of: session.exitRequested) { exit in if exit { onExit() } }
        .alert("\(session.game.title) stopped", isPresented: Binding(get: { session.stopped != nil }, set: { _ in })) {
            Button("Back to the library") { onExit() }
        } message: {
            Text("The game reached something its core can't run yet:\n\(session.stopped ?? "")")
        }
        .onChange(of: phase) { p in
            // Leaving the app pauses under the menu: the game never carries on by itself.
            if p != .active { session.openMenu() }
        }
    }
}

#if !os(macOS)
/// The window title's speed, where there is no window title (iPhone, iPad,
/// Apple TV); only when Settings has Show speed on.
private struct SpeedLabel: View {
    var speed: String
    @AppStorage(SettingKey.showSpeed) private var show = false

    var body: some View {
        if show, !speed.isEmpty {
            Text(speed)
                .font(.caption.monospacedDigit()).padding(6)
                .background(.black.opacity(0.5), in: RoundedRectangle(cornerRadius: 6))
                .foregroundStyle(.white.opacity(0.8))
        }
    }
}
#endif

extension PlayView {
    fileprivate var operatorHint: String {
        #if os(macOS)
        "Operator mode: controller A = Test, B = Service (or F2, F3). Pause to leave."
        #elseif os(tvOS)
        "Operator mode: A = Test, B = Service. Pause, then Leave operator mode."
        #else
        "Operator mode: A = Test, B = Service, or the buttons above. Pause to leave."
        #endif
    }
}

/// Resume, operator mode, the picture or Quit over the stopped game. A
/// controller or the keyboard moves the highlight (GameSession polls them,
/// on Apple TV too: GameView keeps the controller from tvOS's focus); the
/// mouse and touch work too.
struct PauseMenu: View {
    @ObservedObject var session: GameSession

    var body: some View {
        ZStack {
            Color.black.opacity(0.6).ignoresSafeArea()
            #if os(macOS)
            if session.showControls {
                ControlsCard(game: session.game) { session.showControls = false }
            } else {
                items
            }
            #else
            items
            #endif
        }
    }

    private var items: some View {
        VStack(spacing: 16) {
            Text("Paused").font(.largeTitle.bold()).foregroundStyle(.white)
            Text(session.game.title).foregroundStyle(.white.opacity(0.7))
            ForEach(Array(session.menuItems.enumerated()), id: \.offset) { i, item in
                #if os(tvOS)
                row(item, i).frame(minWidth: 520) // not a Button: nothing here takes tvOS's focus
                #else
                Button {
                    session.menuIndex = i
                    session.choose(i)
                } label: {
                    row(item, i).frame(minWidth: 260)
                }
                .buttonStyle(.plain)
                #endif
            }
        }
        .padding(40)
    }

    private func row(_ item: String, _ i: Int) -> some View {
        let on = i == session.menuIndex
        return Text(item).font(.title2.weight(.semibold)).padding(.vertical, 12).padding(.horizontal, 24)
            .background(on ? Self.highlight : Color.white.opacity(0.15), in: RoundedRectangle(cornerRadius: 10))
            .foregroundStyle(on ? Self.highlightText : Color.white)
    }

    // Apple TV's accent colour is white: its highlight is white with dark
    // text, like a focused button there.
    #if os(tvOS)
    private static let highlight = Color.white
    private static let highlightText = Color.black
    #else
    private static let highlight = Color.accentColor
    private static let highlightText = Color.white
    #endif
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
    @AppStorage(SettingKey.appearance) private var appearance = Appearance.dark
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
            Section("Speed") {
                Toggle("Game on its own thread", isOn: $coreThread)
                Text("Try it on a device where a game runs slow: the screen then keeps its own rate. This device only; also in the pause menu.")
                    .font(.caption).foregroundStyle(.secondary)
                #if !os(macOS)
                Toggle("Show speed", isOn: $showSpeed)
                Text("Shows the frame rate and the game's speed in a corner while a game runs.")
                    .font(.caption).foregroundStyle(.secondary)
                #endif
                if let last = UserDefaults.standard.string(forKey: LaunchTimer.key) {
                    LabeledContent("Last start", value: last)
                }
            }
            CabinetSettingsSection(showCabinets: $showCabinets)
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

#if os(iOS)
/// A cabinet button on the touch screen, held while touched.
struct TouchButton: View {
    let title: String
    let bit: UInt32
    let controls: Controls
    @State private var down = false

    var body: some View {
        Text(title)
            .font(.headline).padding(.horizontal, 16).padding(.vertical, 10)
            .background(.white.opacity(down ? 0.5 : 0.2), in: Capsule())
            .foregroundStyle(.white)
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { _ in
                    if !down { down = true; controls.touchButtons |= bit }
                }
                .onEnded { _ in
                    down = false
                    controls.touchButtons &= ~bit
                })
    }
}
#endif

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
