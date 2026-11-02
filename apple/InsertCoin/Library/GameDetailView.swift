import Combine
import GameController
import SwiftUI

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
                VStack(alignment: .leading, spacing: 6) {
                    Text(game.title).font(.title2.bold())
                    Text(game.platform.name).foregroundStyle(.secondary)
                }
            }
            Section("Game Versions") {
                ForEach(Array(game.versions.enumerated()), id: \.element.id) { i, version in
                    row(version)
                        #if !os(tvOS)
                        .listRowBackground(selected == i && GCController.current != nil ? Color.accentColor.opacity(0.2) : nil)
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
            if let history {
                Section("About this game") {
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
            }
        }
        .formStyle(.grouped)
        .navigationTitle(game.title)
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

    private func ready(_ version: GameInfo) -> Bool {
        CoreLibrary.exists(version.id) && (Storage.rom(version.id) != nil || cloud.items[version.id] != nil)
    }

    /// What choosing the version does: play, download, or say why not.
    private func activate(_ version: GameInfo) {
        if CoreLibrary.load(version.id) == nil {
            message = "\(version.id): \(CoreLibrary.unavailable(version.id))."
        } else if ready(version) {
            playing = version
        } else if romServer.isEmpty {
            message = Storage.isFolder(version.id)
                ? "Add its game folder (\(version.id), the folder that holds disk0) or \(version.id).\(GamePackage.ext), or set your ROM server in Settings."
                : "Add its ROM set (\(version.id).zip or .7z), or set your ROM server in Settings."
        } else if downloading == nil {
            download(version)
        }
    }

    private func download(_ version: GameInfo) {
        downloading = version.id
        Task {
            do {
                try await RomServer.download(version.id, from: romServer)
                message = "Downloaded \(version.id)."
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
        return Button { activate(version) } label: {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text(version.detail).font(.title3)
                    Text(version.id).font(.callout).foregroundStyle(.secondary)
                }
                Spacer()
                Label(status, systemImage: ready(version) ? "play.fill" : "arrow.down.circle")
            }
            .padding(.vertical, 12)
        }
    }
    #else
    @ViewBuilder private func row(_ version: GameInfo) -> some View {
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
                Button("Add ROM…") { importing = true }
                if !romServer.isEmpty {
                    Button(downloading == version.id ? downloads.label(version.id) : "Download") { download(version) }
                        .disabled(downloading != nil)
                }
            } else {
                if rom == nil, let inCloud {
                    Image(systemName: inCloud.downloaded ? "icloud" : "icloud.and.arrow.down")
                        .foregroundStyle(.secondary)
                        .help(inCloud.downloaded ? "In iCloud Drive, downloaded" : "In iCloud Drive: downloads when you play")
                }
                Button { playing = version } label: { Label("Play", systemImage: "play.fill") }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.large)
            }
        }
        .padding(.vertical, 10)
    }

    /// A controller on macOS and iOS: up and down choose a version, A plays
    /// (or downloads) it, B goes back to the library.
    private func padTick() {
        guard playing == nil,
              let gp = GCController.current?.extendedGamepad ?? GCController.controllers().compactMap(\.extendedGamepad).first
        else { return }
        if !pad.armed {
            pad.armed = !gp.buttonA.isPressed && !gp.buttonB.isPressed
            return
        }
        let y = gp.dpad.yAxis.value + gp.leftThumbstick.yAxis.value
        let move = y > 0.5 ? -1 : y < -0.5 ? 1 : 0
        if pad.repeats(move) {
            selected = min(max(0, selected + move), game.versions.count - 1)
            optionsFor = game.versions[selected].id
        }
        if pad.pressed("a", gp.buttonA.isPressed) { activate(game.versions[selected]) }
        if pad.pressed("b", gp.buttonB.isPressed) { dismiss() }
    }
    #endif
}
