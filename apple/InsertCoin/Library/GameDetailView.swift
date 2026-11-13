import Combine
import GameController
import SwiftUI
#if os(macOS)
import AppKit
#endif

/// A game's page, behind its library card: its versions (Daytona USA and
/// Daytona USA Revision A), each played, downloaded or added from here, and
/// the options of the one chosen under "Options for", then the game's history.
struct GameDetailView: View {
    let game: ArcadeGame
    @Binding var playing: GameInfo?
    @AppStorage(SettingKey.romServer) private var romServer = ""
    @AppStorage(SettingKey.addToCloud) private var addToCloud = true
    @ObservedObject private var cloud = CloudROMs.shared
    @ObservedObject private var downloads = Downloads.shared
    @ObservedObject private var art = GameArt.shared
    @Environment(\.dismiss) private var dismiss
    @State private var optionsFor: String
    @State private var importing = false
    @State private var message: String?
    @State private var downloading: String?
    @State private var refresh = 0
    /// The version a controller has highlighted (macOS, iOS).
    @State private var selected = 0
    @State private var pad = PadState()
    private let padTimer = Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()
    private let history: GameHistory?

    init(game: ArcadeGame, playing: Binding<GameInfo?>) {
        self.game = game
        _playing = playing
        _optionsFor = State(initialValue: game.versions[0].id)
        history = GameHistory.load(game)
    }

    var body: some View {
        Form {
            Section {
                GamePicture(romset: game.pictureSet)
                    .frame(maxWidth: 480)
                    .frame(maxWidth: .infinity)
                VStack(alignment: .leading, spacing: 6) {
                    Text(game.title).font(.title2.bold())
                    Text(game.platform.name).foregroundStyle(.secondary)
                }
            }
            Section("Game Versions") {
                ForEach(Array(game.versions.enumerated()), id: \.element.id) { i, version in
                    row(version)
                        #if !os(tvOS)
                        .listRowBackground(selected == i && MenuPad.read() != nil ? Color.accentColor.opacity(0.2) : nil)
                        #endif
                }
                if let message {
                    Text(message).font(.caption).foregroundStyle(.secondary)
                }
            }
            .id(refresh)
            if game.versions.count > 1 {
                Section {
                    Picker("Options for", selection: $optionsFor) {
                        ForEach(game.versions) { Text("\($0.detail) (\($0.id))").tag($0.id) }
                    }
                }
            }
            if let version = game.versions.first(where: { $0.id == optionsFor }), CoreLibrary.load(version.id) != nil {
                GameOptionsSections(game: version) { refresh += 1 }
                    .id("\(version.id)-\(refresh)")
            }
            let extras = ArtKind.extras.filter { art.has(artSet, $0) }
            if history != nil || !extras.isEmpty {
                Section("About this game") {
                    if !extras.isEmpty { ArtRow(romset: artSet, kinds: extras) }
                    if let history { historyRows(history) }
                }
            }
        }
        .formStyle(.grouped)
        .navigationTitle(game.title)
        .onAppear { art.want(artSet, ArtKind.extras) }
        // The cabinet's Resume slot from iCloud, when it was left on another device sharing it
        .task {
            for version in game.versions where SaveStates.resumeOn {
                if await SaveSync.syncResume(version.id) { refresh += 1 }
            }
        }
        .onChange(of: playing) { _ in refresh += 1 } // back from a game: its Resume slot changed
        #if !os(tvOS)
        .onReceive(padTimer) { _ in padTick() }
        .fileImporter(isPresented: $importing, allowedContentTypes: Storage.romTypes, allowsMultipleSelection: true) { result in
            switch result {
            case .success(let urls):
                let folder = addToCloud ? cloud.folder : nil
                Task {
                    message = await Storage.addROMs(urls, cloud: folder)
                    refresh += 1
                }
            case .failure(let error):
                message = error.localizedDescription
            }
        }
        #endif
    }

    /// The set the page's extra art is for: the game's first version.
    private var artSet: String { game.versions[0].id }

    @ViewBuilder private func historyRows(_ history: GameHistory) -> some View {
        Text(history.facts).font(.callout).foregroundStyle(.secondary)
            #if os(tvOS)
            .focusable() // the focus engine scrolls only to what it can focus
            #endif
        ForEach(history.paragraphs, id: \.self) { paragraph in
            Text(paragraph)
                .padding(.vertical, 4)
                #if os(tvOS)
                .focusable()
                #endif
        }
        #if !os(tvOS) // Apple TV has no browser
        ForEach(history.links, id: \.url) { Link($0.label, destination: $0.url) }
        #endif
    }

    private func ready(_ version: GameInfo) -> Bool {
        CoreLibrary.exists(version.id) && (Storage.rom(version.id) != nil || cloud.items[version.id] != nil)
    }

    /// What choosing the version does: play, download, or say why not.
    private func activate(_ version: GameInfo) {
        if PCGames.isPC(version.id) { // a PC-based game: its runner and game folder
            if !CoreLibrary.exists(version.id) { message = "\(version.id): \(CoreLibrary.unavailable(version.id))." }
            else if ready(version) { playing = version }
            else { message = "Choose its game folder first." }
            return
        }
        if CoreLibrary.load(version.id) == nil {
            message = "\(version.id): \(CoreLibrary.unavailable(version.id))."
        } else if ready(version) {
            playing = version
        } else if romServer.isEmpty {
            message = Storage.isFolder(version.id)
                ? "Add its game folder (\(version.id), the folder that holds \(Storage.folderMark(version.id))) or \(version.id).\(GamePackage.ext), or set your ROM server in Settings."
                : "Add its ROM set (\(version.id).zip or .7z), or set your ROM server in Settings."
        } else if downloading == nil {
            download(version)
        }
    }

    /// The version from its Resume slot (SaveStates), where it was left.
    private func resume(_ version: GameInfo) {
        guard CoreLibrary.load(version.id) != nil, ready(version) else { return activate(version) }
        SaveStates.resumeNext = version.id
        playing = version
    }

    /// Its Resume slot's date and play time, when it has one to offer.
    private func resumeNote(_ version: GameInfo, _ core: CoreLibrary?) -> String? {
        guard SaveStates.canResume(version.id, library: core) else { return nil }
        return "Where you left off: " + SaveStates.describe(SaveStates.slot(version.id, 0), coreVersion: core?.version ?? "")
    }

    private func download(_ version: GameInfo) {
        downloading = version.id
        Task {
            do {
                try await RomServer.download(version.id, from: romServer)
                message = downloads.downloaded(version.id)
            } catch {
                message = error.localizedDescription
            }
            downloading = nil
            refresh += 1
        }
    }

    #if os(tvOS)
    /// Apple TV: the whole row is one button.
    private func row(_ version: GameInfo) -> some View {
        let core = CoreLibrary.load(version.id)
        let status: String
        if core == nil {
            status = CoreLibrary.unavailable(version.id).hasPrefix("Not") ? "Not in this build" : "Core out of date: rebuild it"
        } else if ready(version) {
            status = "Play"
        } else if downloading == version.id {
            status = downloads.label(version.id)
        } else if romServer.isEmpty {
            status = "Set your ROM server in Settings"
        } else {
            status = "Download from your ROM server"
        }
        let note = ready(version) ? resumeNote(version, core) : nil
        return VStack(spacing: 8) {
            if let note { // Resume, above Play
                Button { resume(version) } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text("Resume \(version.detail)").font(.title3)
                            Text(note).font(.callout).foregroundStyle(.secondary)
                        }
                        Spacer()
                        Label("Resume", systemImage: "playpause.fill")
                    }
                    .padding(.vertical, 12)
                }
            }
            Button { activate(version) } label: {
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(version.detail).font(.title3)
                        Text(version.id).font(.callout).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Label(note == nil ? status : "Play from the start", systemImage: ready(version) ? "play.fill" : "arrow.down.circle")
                }
                .padding(.vertical, 12)
            }
        }
    }
    #else
    @ViewBuilder private func row(_ version: GameInfo) -> some View {
        if PCGames.isPC(version.id) { pcRow(version) } else { coreRow(version) }
    }

    /// A PC-based game (PCGames, Mac only): its game folder, chosen where it
    /// is and never copied, and Play, which starts its runner.
    @ViewBuilder private func pcRow(_ version: GameInfo) -> some View {
        let folder = PCGames.folder(version.id)
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text(version.detail).font(.title3)
                Text(folder.map { "Game folder: \($0.path)" } ?? "Game folder: not chosen").font(.callout).foregroundStyle(.secondary)
            }
            Spacer()
            if PCGames.runner(version.id) == nil {
                Text(CoreLibrary.unavailable(version.id)).font(.caption).foregroundStyle(.secondary)
            } else {
                #if os(macOS)
                Button(folder == nil ? "Choose Game Folder…" : "Change…") { chooseFolder(version) }
                    .buttonStyle(.borderless)
                #endif
                if folder != nil {
                    Button { playing = version } label: { Label("Play", systemImage: "play.fill") }
                        .buttonStyle(.borderedProminent)
                        .controlSize(.large)
                }
            }
        }
        .padding(.vertical, 10)
    }

    #if os(macOS)
    private func chooseFolder(_ version: GameInfo) {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        let program = PCGames.program(version.id)
        panel.message = "Choose the folder that holds \(version.title)" + (program.isEmpty ? "" : " (\(program))")
        panel.prompt = "Choose"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            try PCGames.choose(url, for: version.id)
            message = nil
        } catch {
            message = error.localizedDescription
        }
        refresh += 1
    }
    #endif

    @ViewBuilder private func coreRow(_ version: GameInfo) -> some View {
        let core = CoreLibrary.load(version.id)
        let rom = Storage.rom(version.id)
        let inCloud = cloud.items[version.id]
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text(version.detail).font(.title3)
                Text(version.id).font(.callout).foregroundStyle(.secondary)
            }
            Spacer()
            if core == nil {
                Text(CoreLibrary.unavailable(version.id)).font(.caption).foregroundStyle(.secondary)
            } else if rom == nil && inCloud == nil {
                // Borderless: with the default style a tap anywhere in a list row
                // presses every button in it, so Download also opened the file picker.
                Button("Add ROM…") { importing = true }
                    .buttonStyle(.borderless)
                if !romServer.isEmpty {
                    Button(downloading == version.id ? downloads.label(version.id) : "Download") { download(version) }
                        .buttonStyle(.borderless)
                        .disabled(downloading != nil)
                }
            } else {
                if rom != nil, let problem = downloads.problem(version.id) {
                    Label("Failed the ROM check", systemImage: "exclamationmark.triangle")
                        .font(.caption).foregroundStyle(.orange)
                        .help(problem)
                }
                if rom == nil, let inCloud {
                    Image(systemName: inCloud.downloaded ? "icloud" : "icloud.and.arrow.down")
                        .foregroundStyle(.secondary)
                        .help(inCloud.downloaded ? "In iCloud Drive, downloaded" : "In iCloud Drive: downloads when you play")
                }
                if let note = resumeNote(version, core) {
                    Button { resume(version) } label: { Label("Resume", systemImage: "playpause.fill") }
                        .buttonStyle(.borderedProminent)
                        .controlSize(.large)
                        .help(note)
                    Button { playing = version } label: { Label("Play from the start", systemImage: "play.fill") }
                        .buttonStyle(.bordered)
                        .controlSize(.large)
                } else {
                    Button { playing = version } label: { Label("Play", systemImage: "play.fill") }
                        .buttonStyle(.borderedProminent)
                        .controlSize(.large)
                }
            }
        }
        .padding(.vertical, 10)
    }

    /// A controller on macOS and iOS, or a wheel on the Mac: up and down choose a version, A plays
    /// (or downloads) it, B goes back to the library.
    private func padTick() {
        guard playing == nil, let gp = MenuPad.read() else { return } // a gamepad, or a wheel (MenuPad)
        if !pad.armed {
            pad.armed = !gp.a && !gp.b
            return
        }
        let y = gp.moveY
        let move = y > 0.5 ? -1 : y < -0.5 ? 1 : 0
        if pad.repeats(move) {
            selected = min(max(0, selected + move), game.versions.count - 1)
            optionsFor = game.versions[selected].id
        }
        if pad.pressed("a", gp.a) { activate(game.versions[selected]) }
        if pad.pressed("b", gp.b) { dismiss() }
    }
    #endif
}

/// The game's other art (GameArt) in a row under About this game: the
/// flyer, marquee, cabinet and in-game shot it has, each opening full size.
/// Each is a button, so the Apple TV remote can reach them.
struct ArtRow: View {
    let romset: String
    let kinds: [ArtKind]
    @State private var shown: ArtKind?

    #if os(tvOS)
    private static let height: CGFloat = 220
    #else
    private static let height: CGFloat = 120
    #endif

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 16) {
                ForEach(kinds) { kind in
                    if let image = GameArt.shared.image(romset, kind) {
                        Button { shown = kind } label: {
                            VStack(spacing: 6) {
                                Image(decorative: image, scale: 1).resizable().scaledToFit()
                                    .frame(height: Self.height)
                                    .clipShape(RoundedRectangle(cornerRadius: 6))
                                Text(kind.name).font(.caption).foregroundStyle(.secondary)
                            }
                        }
                        #if os(tvOS)
                        .buttonStyle(.card)
                        #else
                        .buttonStyle(.plain)
                        #endif
                        .accessibilityLabel(kind.name)
                    }
                }
            }
            .padding(.vertical, 8)
        }
        .sheet(item: $shown) { kind in
            ArtViewer(romset: romset, kind: kind) { shown = nil }.appAppearance()
        }
    }
}

/// One piece of art, as large as the screen allows.
struct ArtViewer: View {
    let romset: String
    let kind: ArtKind
    let close: () -> Void

    var body: some View {
        VStack(spacing: 12) {
            if let image = GameArt.shared.image(romset, kind) {
                Image(decorative: image, scale: 1).resizable().scaledToFit()
            }
            HStack {
                Text(kind.name).font(.headline)
                Spacer()
                Text("From the Arcade Database or libretro-thumbnails").font(.caption).foregroundStyle(.secondary)
                #if !os(tvOS) // the remote's Menu button closes it
                Button("Done", action: close).keyboardShortcut(.cancelAction)
                #endif
            }
        }
        .padding()
        #if os(macOS)
        .frame(minWidth: 640, idealWidth: 900, minHeight: 520, idealHeight: 760)
        #endif
    }
}
