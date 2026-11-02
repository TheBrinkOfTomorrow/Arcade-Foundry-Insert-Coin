import Combine
import GameController
import SwiftUI
import UniformTypeIdentifiers

/// The library: a card per game and platform (ArcadeGame), filtered by
/// platform and, optionally, to the playable ones; as cards or as a list of
/// compact rows (many games on screen at once). A card or row opens the game's page (GameDetailView), where its
/// versions are played and its options changed.
struct LibraryView: View {
    @Binding var playing: GameInfo?
    @Binding var path: [String] // the game pages open over the library (ArcadeGame ids)
    @AppStorage(SettingKey.romServer) private var romServer = ""
    @AppStorage(SettingKey.addToCloud) private var addToCloud = true
    @AppStorage(SettingKey.platformFilter) private var platform = "all"
    @AppStorage(SettingKey.playableOnly) private var playableOnly = false
    @AppStorage(SettingKey.libraryList) private var listView = false
    @AppStorage(SettingKey.librarySort) private var sort = LibrarySort.title
    #if os(iOS)
    @Environment(\.horizontalSizeClass) private var sizeClass
    #endif
    @ObservedObject private var cloud = CloudROMs.shared
    @ObservedObject private var downloads = Downloads.shared
    @State private var importing = false
    @State private var message: String?
    @State private var downloading: String?
    @State private var refresh = 0
    @State private var appSettings = false
    /// The card a controller has highlighted (macOS, iOS), and the grid's
    /// column count so up and down move a row.
    @State private var selected: Int?
    @State private var gridColumns = 1
    #if !os(tvOS)
    /// Columns added by pinching (#173): 0 is the largest cards, as the app
    /// opens; not kept between launches.
    @State private var smaller = 0
    @State private var gridWidth: CGFloat = 0
    @State private var pinch: CGFloat = 1
    /// A pinch is on, or just ended: the cards take no clicks or taps, or
    /// the end of a pinch over a card opened its game (#179).
    @State private var pinching = false
    /// When the last pinch ended: a card ignores a click soon after (#185).
    @State private var pinchEnded = Date.distantPast
    #endif
    @State private var pad = PadState()
    /// The connected controller's symbols for X, Y, L1, R1, L2, R2 (#199):
    /// empty with no controller.
    @State private var padSymbols: [String] = []
    private let padTimer = Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()

    #if os(tvOS)
    private static let cardWidth: CGFloat = 360 // four across (#181)
    #else
    private static let cardWidth: CGFloat = 260
    #endif
    #if os(tvOS)
    private let columns = [GridItem(.adaptive(minimum: Self.cardWidth), spacing: 20)]
    #else
    private static let smallestCard: CGFloat = 130 // the title still fits (compact cards)
    private var columns: [GridItem] { Array(repeating: GridItem(.flexible(), spacing: 20), count: gridColumns) }

    /// The cards are narrow enough for the compact layout.
    private var compactCards: Bool {
        let n = CGFloat(max(1, gridColumns))
        return (gridWidth - 48 - 20 * (n - 1)) / n < 220
    }

    /// The grid's columns for this width: today's, plus those pinched in.
    private func layOut(_ width: CGFloat) {
        gridWidth = width
        smaller = min(smaller, max(0, Self.columnCount(width, card: Self.smallestCard) - Self.columnCount(width)))
        gridColumns = Self.columnCount(width) + smaller
    }

    /// One pinch step: in for more, smaller cards, out for fewer, larger ones.
    private func zoom(_ step: Int) {
        let most = max(0, Self.columnCount(gridWidth, card: Self.smallestCard) - Self.columnCount(gridWidth))
        let new = max(0, min(most, smaller + step))
        guard new != smaller else { return }
        withAnimation(.easeInOut(duration: 0.2)) {
            smaller = new
            gridColumns = Self.columnCount(gridWidth) + new
        }
    }
    #endif

    /// The cards the filters show: the platform's, and only the playable
    /// ones (a core in this build and a ROM set here or in iCloud Drive) when
    /// that is on; in the chosen order.
    private var shown: [ArcadeGame] {
        ArcadeGame.sorted(ArcadeGame.all.filter { game in
            (Platform(rawValue: platform).map { game.platform == $0 } ?? true) && (!playableOnly || status(game).playable)
        }, by: sort)
    }

    var body: some View {
        GeometryReader { geo in
            ScrollViewReader { scroller in
                ScrollView {
                    // Exactly as wide as the screen: with anything wider,
                    // scrolling a highlighted card into view also scrolls
                    // sideways and the whole library slides (#136).
                    VStack {
                        #if os(tvOS)
                        tvHeader
                        #else
                        if !padSymbols.isEmpty { // the toolbar takes no controller: its buttons (#184)
                            padHint
                                .font(.caption).foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(.horizontal, 24).padding(.top, 8)
                        }
                        #endif
                        let games = shown
                        if games.isEmpty {
                            let name = Platform(rawValue: platform).map { "\($0.name) " } ?? ""
                            Text(playableOnly ? "No \(name)games to play yet: add their ROM sets (Add ROM), download them from your ROM server, or show All."
                                              : "No \(name)games yet.")
                                .foregroundStyle(.secondary).padding(24)
                        }
                        Group {
                            if listView {
                                #if os(tvOS) // columns of rows: a row across the whole TV is mostly empty
                                LazyVGrid(columns: [GridItem(.adaptive(minimum: 560), spacing: 30)], spacing: Self.rowSpacing) {
                                    ForEach(games) { row($0).id($0.id) }
                                }
                                #else
                                LazyVStack(spacing: Self.rowSpacing) {
                                    ForEach(Array(games.enumerated()), id: \.element.id) { i, game in
                                        row(game)
                                            .overlay(RoundedRectangle(cornerRadius: 8)
                                                .stroke(Color.accentColor, lineWidth: selected == i ? 3 : 0))
                                            .id(game.id)
                                    }
                                }
                                #endif
                            } else {
                                LazyVGrid(columns: columns, spacing: 20) {
                                    ForEach(Array(games.enumerated()), id: \.element.id) { i, game in
                                        card(game)
                                            .overlay(RoundedRectangle(cornerRadius: 12)
                                                .stroke(Color.accentColor, lineWidth: selected == i ? 4 : 0))
                                            .id(game.id)
                                    }
                                }
                                #if !os(tvOS)
                                .allowsHitTesting(!pinching)
                                #endif
                            }
                        }
                        .padding(24)
                        .id(refresh)
                        #if os(tvOS)
                        .focusSection()
                        #endif
                        #if !os(tvOS) // Apple TV shows it in the row above
                        if let message {
                            Text(message).foregroundStyle(.secondary).padding(.horizontal, 24)
                        }
                        #endif
                    }
                    .frame(width: geo.size.width)
                }
                #if !os(tvOS)
                // Pinch the cards smaller or larger, a column at a time (#173).
                .simultaneousGesture(MagnificationGesture()
                    .onChanged { scale in
                        if !pinching { pinching = true }
                        if scale / pinch < 0.8 { zoom(1); pinch = scale } else if scale / pinch > 1.25 { zoom(-1); pinch = scale }
                    }
                    .onEnded { _ in
                        pinch = 1
                        pinchEnded = Date()
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.4) { pinching = false }
                    })
                #endif
                .onChange(of: selected) { i in
                    if let i, i < shown.count { withAnimation { scroller.scrollTo(shown[i].id) } }
                }
            }
            #if os(tvOS)
            .onAppear { gridColumns = Self.columnCount(geo.size.width) }
            .onChange(of: geo.size.width) { gridColumns = Self.columnCount($0) }
            #else
            .onAppear { layOut(geo.size.width) }
            .onChange(of: geo.size.width) { layOut($0) }
            #endif
        }
        // A platform the filter no longer has (Other, removed): All.
        .onAppear { if platform != "all" && Platform(rawValue: platform) == nil { platform = "all" } }
        // After this pass draws: the library's first frame (#202).
        .onAppear { DispatchQueue.main.async { LaunchTimer.libraryShown() } }
        .onChange(of: platform) { _ in selected = nil }
        .onChange(of: playableOnly) { _ in selected = nil }
        .onChange(of: listView) { _ in selected = nil }
        .onChange(of: sort) { _ in selected = nil }
        #if !os(tvOS)
        .onReceive(padTimer) { _ in padTick() }
        #endif
        #if os(tvOS) // the logo heads the row above the games: a title would float over the cards (#177)
        .toolbar(.hidden, for: .navigationBar)
        #elseif os(iOS) // the logo is the title: no name beside it on iPad (#199)
        .navigationTitle("")
        #else
        .navigationTitle("Arcade Foundry: Insert Coin")
        #endif
        #if os(iOS) // a small logo in the bar instead of the title (#138)
        .navigationBarTitleDisplayMode(.inline)
        #endif
        .navigationDestination(for: String.self) { id in
            if let game = ArcadeGame.with(id: id) {
                GameDetailView(game: game, playing: $playing)
            }
        }
        .toolbar {
            #if !os(tvOS)
            #if os(iOS)
            if sizeClass == .compact { // iPhone: the bar is narrow (upright above all)
                ToolbarItem { filterMenu }
            } else {
                // iOS 26 puts each item in a glass pill; a segmented control
                // has its own, so the bar's is hidden behind those (#171).
                // The platform and sort menus each in a pill of their own, as on
                // the Mac, not in one with the buttons after them (#191).
                if #available(iOS 26, *) {
                    ToolbarItem { viewPicker.pickerStyle(.segmented).fixedSize() }
                        .sharedBackgroundVisibility(.hidden)
                    ToolbarItem { playablePicker.pickerStyle(.segmented).fixedSize() }
                        .sharedBackgroundVisibility(.hidden)
                    ToolbarItem { centredMenu("Platform", platformName) { platformPicker }.tint(.primary) }
                    ToolbarSpacer(.fixed)
                    ToolbarItem { centredMenu("Sort by", sort.name) { sortPicker }.tint(.primary) }
                    ToolbarSpacer(.fixed)
                } else {
                    ToolbarItem { viewPicker.pickerStyle(.segmented).fixedSize() }
                    ToolbarItem { playablePicker.pickerStyle(.segmented).fixedSize() }
                    ToolbarItem { centredMenu("Platform", platformName) { platformPicker }.tint(.primary) }
                    ToolbarItem { centredMenu("Sort by", sort.name) { sortPicker }.tint(.primary) }
                }
            }
            #else
            ToolbarItem { viewPicker.pickerStyle(.segmented).fixedSize() }
            ToolbarItem { playablePicker.pickerStyle(.segmented).fixedSize() }
            // Menus with their text centred, not left-aligned in the pill (#184).
            ToolbarItem { centredMenu("Platform", platformName) { platformPicker } }
            ToolbarItem { centredMenu("Sort by", sort.name) { sortPicker }.help("The order of the games") }
            #endif
            ToolbarItem {
                Button { importing = true } label: { Label("Add ROM", systemImage: "plus") }
            }
            if !romServer.isEmpty && !missing.isEmpty {
                ToolbarItem {
                    Button { Task { await downloadMissing() } } label: {
                        Label("Download all from the ROM server", systemImage: "arrow.down.circle")
                    }
                    .disabled(downloading != nil)
                }
            }
            #endif
            #if os(iOS)
            // iPhone: the AF mark (the bar is narrow), centred; iPad: with
            // INSERT COIN beside it, always at the left (centred, it moved
            // as the buttons beside it changed width, #190).
            if sizeClass == .compact {
                ToolbarItem(placement: .principal) {
                    Image("NavMark").accessibilityLabel("Arcade Foundry: Insert Coin")
                }
            } else if #available(iOS 26, *) {
                ToolbarItem(placement: .topBarLeading) {
                    Image("NavLogo").accessibilityLabel("Arcade Foundry: Insert Coin")
                }
                .sharedBackgroundVisibility(.hidden)
            } else {
                ToolbarItem(placement: .navigationBarLeading) {
                    Image("NavLogo").accessibilityLabel("Arcade Foundry: Insert Coin")
                }
            }
            #endif
            #if os(iOS) // macOS has its Settings window, Apple TV a row above the games
            ToolbarItem {
                Button { appSettings = true } label: { Label("Settings", systemImage: "gearshape") }
            }
            #endif
        }
        .sheet(isPresented: $appSettings) {
            NavigationStack {
                AppSettingsView().toolbar {
                    ToolbarItem(placement: .confirmationAction) { Button("Done") { appSettings = false } }
                }
            }
            .appAppearance() // a sheet is its own presentation: without this Apple TV draws white text on the focus highlight (#261)
        }
        #if !os(tvOS)
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
        #if os(tvOS)
        // Apple TV keeps its ROM sets in Caches, which the system may clear:
        // fetch what is missing whenever the library shows, or the server
        // address arrives from the user's other devices.
        .task { await downloadMissing() }
        .onChange(of: romServer) { _ in Task { await downloadMissing() } }
        #endif
    }

    /// The platform filter: All, Model 1, Model 2, ...
    private var platformPicker: some View {
        Picker("Platform", selection: $platform) {
            Text("All").tag("all")
            ForEach(Platform.allCases) { Text($0.name).tag($0.rawValue) }
        }
    }

    #if !os(tvOS)
    private var platformName: String {
        Platform(rawValue: platform)?.name ?? "All"
    }

    /// A toolbar menu showing what it is and its choice, centred (#184, #191),
    /// with the picker's choices (checked) inside.
    private func centredMenu<P: View>(_ name: String, _ choice: String, @ViewBuilder picker: () -> P) -> some View {
        Menu {
            picker().pickerStyle(.inline).labelsHidden()
        } label: {
            Text(label(name, choice)).lineLimit(1).fixedSize().frame(minWidth: 110)
        }
        .accessibilityLabel("\(name): \(choice)")
    }

    /// "Name: choice", the name in the secondary colour (one Text: a menu's
    /// label shows one).
    private func label(_ name: String, _ choice: String) -> AttributedString {
        var label = AttributedString("\(name): ")
        label.foregroundColor = .secondary
        return label + AttributedString(choice)
    }
    #endif

    #if os(iOS)
    /// The three filters in one menu, for a narrow toolbar.
    private var filterMenu: some View {
        Menu {
            viewPicker.pickerStyle(.menu)
            playablePicker.pickerStyle(.menu)
            platformPicker.pickerStyle(.menu)
            sortPicker.pickerStyle(.menu)
        } label: {
            Label("Show", systemImage: "line.3.horizontal.decrease.circle")
        }
    }
    #endif

    /// The order: A–Z, year, platform or recently played.
    private var sortPicker: some View {
        Picker("Sort", selection: $sort) {
            ForEach(LibrarySort.allCases) { Text($0.name).tag($0) }
        }
        .help("The order of the games")
    }

    /// Cards, or a list of compact rows.
    private var viewPicker: some View {
        Picker("View", selection: $listView) {
            Label("Cards", systemImage: "square.grid.2x2").tag(false)
            Label("List", systemImage: "list.bullet").tag(true)
        }
        .help("Show the games as cards, or as a list that fits many on the screen")
    }

    /// All the games, or only those that can be played now.
    private var playablePicker: some View {
        Picker("Show", selection: $playableOnly) {
            Text("All").tag(false)
            Text("Playable").tag(true)
        }
        .help("Show every game, or only those with a core in this build and a ROM set here or in iCloud Drive")
    }

    /// Games this build has a core for, with no ROM set here or in iCloud Drive.
    private var missing: [GameInfo] {
        GameInfo.all.filter { CoreLibrary.exists($0.id) && Storage.rom($0.id) == nil && cloud.items[$0.id] == nil }
    }

    /// What a card says about its game: playable, its ROM set missing, or no core.
    private func status(_ game: ArcadeGame) -> (text: String, playable: Bool) {
        let built = game.versions.filter { CoreLibrary.exists($0.id) }
        if built.isEmpty {
            return (game.versions.contains { CoreLibrary.isOutdated($0.id) } ? "Core out of date: rebuild it" : "Not in this build", false)
        }
        let ready = built.filter { Storage.rom($0.id) != nil || cloud.items[$0.id] != nil }
        if ready.isEmpty {
            if let id = downloading, built.contains(where: { $0.id == id }) { return (downloads.label(id), false) }
            return ("No ROM set yet", false)
        }
        return (game.versions.count > 1 ? "\(ready.count) of \(game.versions.count) versions ready" : "Ready to play", true)
    }

    /// The card's picture: the first version with one.
    private func pictureSet(_ game: ArcadeGame) -> String {
        game.versions.first { Pictures.exists($0.id) }?.id ?? game.versions[0].id
    }

    private func subtitle(_ game: ArcadeGame) -> String {
        game.versions.count > 1 ? "\(game.platform.name) · \(game.versions.count) versions"
                                : "\(game.platform.name) · \(game.versions[0].detail)"
    }

    #if os(tvOS)
    /// Apple TV: Settings, the filters and Download all in a row above the
    /// games (the navigation bar squeezes them), the last message under
    /// them. Each filter is a button that steps to its next value: a
    /// segmented picker changes as the focus moves across it.
    private var tvHeader: some View {
        VStack(alignment: .leading, spacing: 12) {
            Image("TVLogo") // NavLogo drawn at twice the bar's size, centred (#181)
                .frame(maxWidth: .infinity)
                .accessibilityLabel("Arcade Foundry: Insert Coin")
            HStack(spacing: 30) {
                Button { appSettings = true } label: { Label("Settings", systemImage: "gearshape") }
                    .fixedSize()
                Button { listView.toggle() } label: {
                    Label(listView ? "View: List" : "View: Cards", systemImage: listView ? "list.bullet" : "square.grid.2x2")
                }
                .fixedSize()
                Button { playableOnly.toggle() } label: {
                    Label(playableOnly ? "Show: Playable" : "Show: All", systemImage: "line.3.horizontal.decrease.circle")
                }
                .fixedSize()
                Button { platform = Self.nextPlatform(after: platform) } label: {
                    Label("Platform: \(Platform(rawValue: platform)?.name ?? "All")", systemImage: "cpu")
                }
                .fixedSize()
                Button { sort = Self.nextSort(after: sort) } label: {
                    Label("Sort: \(sort.name)", systemImage: "arrow.up.arrow.down")
                }
                .fixedSize()
                if !romServer.isEmpty && !missing.isEmpty {
                    Button { Task { await downloadMissing() } } label: {
                        Label("Download all", systemImage: "arrow.down.circle")
                    }
                    .fixedSize()
                    .disabled(downloading != nil)
                }
                Spacer(minLength: 0)
            }
            .focusSection()
            if let message {
                Text(message).font(.callout).foregroundStyle(.secondary).lineLimit(2)
            }
        }
        .padding(.horizontal, 24)
        .padding(.top, 24)
    }

    /// Apple TV: the whole card is one focusable link to the game's page.
    private func card(_ game: ArcadeGame) -> some View {
        let s = status(game)
        return NavigationLink(value: game.id) {
            VStack(alignment: .leading, spacing: 10) {
                GamePicture(romset: pictureSet(game))
                Text(game.title).font(.title3.bold())
                Text(subtitle(game)).font(.caption).foregroundStyle(.secondary)
                Spacer(minLength: 8)
                Label(s.text, systemImage: s.playable ? "play.fill" : "arrow.down.circle").font(.callout)
            }
            .frame(maxWidth: .infinity, minHeight: 200, alignment: .topLeading)
            .padding(24)
            .opacity(s.text == "Not in this build" ? 0.6 : 1)
        }
        // Never disabled: the focus engine skips disabled views, and the
        // cards around them could not be reached.
        .buttonStyle(.card)
    }
    #else
    /// A compact card (pinched small) keeps the picture and the whole title,
    /// with the status as a badge on the picture.
    private func card(_ game: ArcadeGame) -> some View {
        let s = status(game)
        let compact = compactCards
        return Button { open(game) } label: {
            VStack(alignment: .leading, spacing: compact ? 6 : 8) {
                GamePicture(romset: pictureSet(game))
                    .overlay(alignment: .bottomTrailing) {
                        if compact {
                            Image(systemName: s.playable ? "play.fill" : "arrow.down.circle")
                                .font(.caption).foregroundStyle(.white).padding(5)
                                .background(.black.opacity(0.6), in: Circle()).padding(5)
                                .accessibilityLabel(s.text)
                        }
                    }
                Text(game.title).font(compact ? .subheadline.bold() : .title3.bold())
                    .lineLimit(compact ? 2 : nil).minimumScaleFactor(compact ? 0.75 : 1)
                if !compact {
                    Text(subtitle(game)).font(.caption).foregroundStyle(.secondary)
                    Spacer(minLength: 8)
                    Label(s.text, systemImage: s.playable ? "play.fill" : "arrow.down.circle")
                        .font(.caption).foregroundStyle(s.playable ? Color.primary : Color.secondary)
                }
            }
            .frame(maxWidth: .infinity, minHeight: compact ? 0 : 130, alignment: .topLeading)
            .padding(compact ? 10 : 16)
            .background(.quaternary.opacity(0.5), in: RoundedRectangle(cornerRadius: 12))
            .contentShape(RoundedRectangle(cornerRadius: 12))
        }
        .buttonStyle(.plain)
    }

    /// A card's click or tap: its game's page, unless it is the end of a
    /// pinch (#185). Hit testing alone let a pinch at the largest size through.
    private func open(_ game: ArcadeGame) {
        guard !pinching, Date().timeIntervalSince(pinchEnded) > 0.6 else { return }
        #if os(macOS)
        switch NSApp.currentEvent?.type {
        case .magnify, .gesture, .beginGesture, .endGesture: return
        default: break
        }
        #endif
        path.append(game.id)
    }
    #endif

    #if os(tvOS)
    private static let rowSpacing: CGFloat = 12
    private static let rowPicture: CGFloat = 96
    #else
    private static let rowSpacing: CGFloat = 4
    private static let rowPicture: CGFloat = 48
    #endif

    /// The list view's row: a small picture, the title, platform and status on one line.
    private func row(_ game: ArcadeGame) -> some View {
        let s = status(game)
        return NavigationLink(value: game.id) {
            HStack(spacing: 12) {
                GamePicture(romset: pictureSet(game)).frame(width: Self.rowPicture)
                VStack(alignment: .leading, spacing: 2) {
                    Text(game.title).font(.headline).lineLimit(1)
                    Text(subtitle(game)).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                }
                Spacer(minLength: 12)
                Label(s.text, systemImage: s.playable ? "play.fill" : "arrow.down.circle")
                    .font(.caption).foregroundStyle(s.playable ? Color.primary : Color.secondary)
                    .lineLimit(1)
                    .fixedSize() // the title gives way, not the status
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 4)
            .frame(maxWidth: .infinity, alignment: .leading)
            #if !os(tvOS)
            .background(.quaternary.opacity(0.5), in: RoundedRectangle(cornerRadius: 8))
            .contentShape(RoundedRectangle(cornerRadius: 8))
            #endif
            .opacity(s.text == "Not in this build" ? 0.6 : 1)
        }
        #if os(tvOS)
        .buttonStyle(.card)
        #else
        .buttonStyle(.plain)
        #endif
    }

    private static func columnCount(_ width: CGFloat, card: CGFloat = cardWidth) -> Int {
        max(1, Int((width - 48 + 20) / (card + 20)))
    }

    /// The library with a controller on macOS and iOS (Apple TV has focus):
    /// the d-pad or left stick moves the highlight, A opens the game's page,
    /// Menu the app's settings (iOS); X, Y, the shoulders and triggers the
    /// toolbar's choices.
    private func padTick() {
        let gp = GCController.current?.extendedGamepad ?? GCController.controllers().compactMap(\.extendedGamepad).first
        let symbols = gp.map { g in
            [g.buttonX, g.buttonY, g.leftShoulder, g.rightShoulder, g.leftTrigger, g.rightTrigger].map { $0.sfSymbolsName ?? "" }
        } ?? []
        if padSymbols != symbols { padSymbols = symbols }
        guard playing == nil, path.isEmpty, !appSettings, let gp else { return }
        if !pad.armed {
            pad.armed = !gp.buttonA.isPressed && !gp.buttonB.isPressed && !gp.buttonMenu.isPressed
            return
        }
        let x = gp.dpad.xAxis.value + gp.leftThumbstick.xAxis.value
        let y = gp.dpad.yAxis.value + gp.leftThumbstick.yAxis.value
        var move = 0
        if x > 0.5 { move = 1 } else if x < -0.5 { move = -1 } else if y > 0.5 { move = listView ? -1 : -gridColumns } else if y < -0.5 { move = listView ? 1 : gridColumns }
        let games = shown
        if pad.repeats(move), !games.isEmpty {
            selected = selected.map { min(max(0, $0 + move), games.count - 1) } ?? 0
        }
        if pad.pressed("a", gp.buttonA.isPressed), let i = selected, i < games.count {
            pad.armed = false // the page waits for A to be let go
            path.append(games[i].id)
        }
        // The toolbar's choices, which a controller cannot reach (#184).
        if pad.pressed("x", gp.buttonX.isPressed) { listView.toggle() }
        if pad.pressed("y", gp.buttonY.isPressed) { playableOnly.toggle() }
        if pad.pressed("r1", gp.rightShoulder.isPressed) { platform = Self.nextPlatform(after: platform) }
        if pad.pressed("l1", gp.leftShoulder.isPressed) { platform = Self.nextPlatform(after: platform, by: -1) }
        if pad.pressed("r2", gp.rightTrigger.isPressed) { sort = Self.nextSort(after: sort) }
        if pad.pressed("l2", gp.leftTrigger.isPressed) { sort = Self.nextSort(after: sort, by: -1) }
        #if os(iOS)
        if pad.pressed("menu", gp.buttonMenu.isPressed) { appSettings = true }
        #endif
    }

    /// The toolbar's controller buttons, drawn as the connected controller
    /// marks them (Xbox's X is PlayStation's square), its name if it has no symbol.
    private var padHint: Text {
        func glyph(_ i: Int, _ name: String) -> Text {
            i < padSymbols.count && !padSymbols[i].isEmpty ? Text(Image(systemName: padSymbols[i])) : Text(name)
        }
        return Text("\(glyph(0, "X")) cards or list · \(glyph(1, "Y")) all or playable · \(glyph(2, "L1")) \(glyph(3, "R1")) platform · \(glyph(4, "L2")) \(glyph(5, "R2")) sort")
    }

    /// The platform filter's next (or previous) choice, round to All again.
    private static func nextPlatform(after platform: String, by step: Int = 1) -> String {
        let all = ["all"] + Platform.allCases.map(\.rawValue)
        return all[((all.firstIndex(of: platform) ?? 0) + step + all.count) % all.count]
    }

    private static func nextSort(after sort: LibrarySort, by step: Int = 1) -> LibrarySort {
        let all = LibrarySort.allCases
        return all[((all.firstIndex(of: sort) ?? 0) + step + all.count) % all.count]
    }

    /// Downloads every missing ROM set the server has.
    private func downloadMissing() async {
        let games = missing
        guard !romServer.isEmpty, !games.isEmpty, downloading == nil else { return }
        var got: [String] = [], failed: [String] = []
        var unreachable: String?
        for game in games {
            downloading = game.id
            do {
                try await RomServer.download(game.id, from: romServer)
                got.append(game.id)
            } catch let error as RomServer.Unreachable {
                unreachable = error.localizedDescription
                break
            } catch {
                failed.append(game.id)
            }
        }
        downloading = nil
        refresh += 1
        var parts: [String] = []
        if !got.isEmpty { parts.append("Downloaded \(got.joined(separator: ", ")).") }
        if !failed.isEmpty { parts.append("Not on the ROM server: \(failed.joined(separator: ", ")).") }
        if let unreachable { parts.append(unreachable) }
        message = parts.joined(separator: " ")
    }
}

/// A gun game's crosshair, shown where a controller aims (CrosshairView).
struct CrosshairToggle: View {
    @AppStorage private var on: Bool

    init(romset: String) { _on = AppStorage(wrappedValue: true, SettingKey.crosshair(romset)) }

    var body: some View {
        Toggle("Show the crosshair", isOn: $on)
        Text("Where a controller aims the gun. The mouse pointer and touches show their own place.")
            .font(.caption).foregroundStyle(.secondary)
    }
}

/// The game's controller layout (ButtonMap): each action and the button
/// that does it. Choosing one waits for the controller button to use
/// instead; a button another action had swaps with it.
struct ControllerMapSection: View {
    let game: GameInfo
    @State private var map = ButtonMap.identity
    @State private var listening: PadButton?
    /// The button that chose the row is up again (it is not the answer).
    @State private var armed = false
    private let timer = Timer.publish(every: 0.05, on: .main, in: .common).autoconnect()

    var body: some View {
        Section("Controller") {
            if game.showsCrosshair { CrosshairToggle(romset: game.id) }
            ForEach(ButtonMap.actions(for: game)) { item in
                Button {
                    listening = listening == item.button ? nil : item.button
                    armed = false
                } label: {
                    LabeledContent(item.name, value: listening == item.button ? "Press a button…" : map[item.button].name)
                }
                #if os(tvOS)
                .modifier(SwallowBack(active: listening == item.button)) // B is the answer, not Back
                #endif
            }
            Text("Choose an action, then press the controller button for it. The sticks, the d-pad, the pause menu and operator mode keep their buttons.")
                .font(.caption).foregroundStyle(.secondary)
            if !map.isDefault {
                Button("Reset to the default buttons") {
                    map = .identity
                    map.save(game.id)
                    listening = nil
                }
            }
        }
        .onAppear { map = ButtonMap.load(game.id) }
        .onReceive(timer) { _ in poll() }
    }

    private func poll() {
        guard let target = listening,
              let pad = GCController.current?.extendedGamepad ?? GCController.controllers().compactMap(\.extendedGamepad).first
        else { return }
        let down = PadButton.allCases.first { $0.element(of: pad)?.isPressed == true }
        if !armed {
            armed = down == nil
            return
        }
        guard let down else { return }
        if let other = PadButton.allCases.first(where: { $0 != target && map[$0] == down }) {
            map[other] = map[target]
        }
        map[target] = down
        map.save(game.id)
        listening = nil
    }
}

/// A game's picture (Pictures), filling a 4:3 frame; a placeholder until
/// it has one.
struct GamePicture: View {
    let romset: String

    var body: some View {
        Color.black
            .aspectRatio(4.0 / 3.0, contentMode: .fit)
            .overlay {
                if let image = Pictures.image(romset) {
                    Image(decorative: image, scale: 1).resizable().scaledToFill()
                } else {
                    Image(systemName: "gamecontroller").font(.largeTitle).foregroundStyle(.secondary)
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}

/// Edge detection and key repeat for the library's controller polling.
final class PadState {
    /// Off until every button is up, so a button still held from the game
    /// (A on the pause menu's Quit) does not start one again.
    var armed = false
    private var held = Set<String>()
    private var lastMove = 0
    private var moveTicks = 0

    func pressed(_ name: String, _ down: Bool) -> Bool {
        defer { if down { held.insert(name) } else { held.remove(name) } }
        return down && !held.contains(name)
    }

    /// A move on its first tick, then repeating after 0.4 s every 0.15 s.
    func repeats(_ move: Int) -> Bool {
        defer { lastMove = move }
        guard move != 0 else { moveTicks = 0; return false }
        if move != lastMove { moveTicks = 0 }
        moveTicks += 1
        return moveTicks == 1 || (moveTicks > 4 && moveTicks % 2 == 0)
    }
}

/// One version's options, as sections of its game's page: link play,
/// picture, screen, core, controller and ROM set. Saved as they change; `changed`
/// tells the page when the ROM set moved or went.
struct GameOptionsSections: View {
    let game: GameInfo
    var changed: () -> Void = {}
    @State private var enabled: Bool
    @State private var port: Int
    @State private var next: String
    @State private var frameSync: Bool
    @ObservedObject private var cloud = CloudROMs.shared
    @ObservedObject private var discovery = LinkDiscovery.shared
    @State private var busy = false
    @State private var problem: String?
    @State private var choosingPicture = false
    @State private var pictureChanged = 0
    @AppStorage private var ownReplay: String
    @AppStorage(SettingKey.recordReplay) private var recordReplays = false

    init(game: GameInfo, changed: @escaping () -> Void = {}) {
        self.game = game
        self.changed = changed
        let l = LinkSettings(romset: game.id)
        _enabled = State(initialValue: l.enabled)
        _port = State(initialValue: l.port)
        _next = State(initialValue: l.next)
        _frameSync = State(initialValue: l.frameSync)
        _ownReplay = AppStorage(wrappedValue: "", ReplayRecorder.key(game.id))
    }

    var body: some View {
        if game.link {
            Section("Link play") {
                Toggle("Link to other cabinets", isOn: $enabled)
                    .onChange(of: enabled) { _ in save() }
                TextField("This cabinet's port", value: $port, format: .number.grouping(.never))
                    .onChange(of: port) { _ in save() }
                TextField("Next cabinet (host:port)", text: $next)
                    .onChange(of: next) { _ in save() }
                let peers = discovery.peers.filter { $0.ports[game.id] != nil }
                if peers.isEmpty {
                    Text("No other cabinets found on your network: open Arcade Foundry: Insert Coin on them (same Wi-Fi), or type the next cabinet's address.")
                        .font(.caption).foregroundStyle(.secondary)
                }
                ForEach(peers) { peer in
                    Button("Next cabinet: \(peer.name)") { pick(peer) }
                }
                Toggle("Frame sync (every cabinet waits for the master)", isOn: $frameSync)
                    .onChange(of: frameSync) { _ in save() }
                Text(help).font(.caption).foregroundStyle(.secondary)
            }
        }
        #if !os(tvOS)
        if !(game.link && enabled) {
            Section("Replays") {
                Toggle("Own replay setting for this game", isOn: Binding(
                    get: { !ownReplay.isEmpty },
                    set: { ownReplay = $0 ? (recordReplays ? "on" : "off") : "" }))
                if ownReplay.isEmpty {
                    Text("Uses Record replays in Settings (\(recordReplays ? "on" : "off")).").font(.caption).foregroundStyle(.secondary)
                } else {
                    Toggle("Record replay", isOn: Binding(get: { ownReplay == "on" }, set: { ownReplay = $0 ? "on" : "off" }))
                }
            }
        }
        #endif
        Section("Picture") {
            GamePicture(romset: game.id).frame(maxWidth: 320).id(pictureChanged)
            Text("Taken from the game after half a minute of play, or with \"Use this screen as the picture\" in the pause menu.")
                .font(.caption).foregroundStyle(.secondary)
            #if !os(tvOS)
            Button("Choose an image…") { choosingPicture = true }
                .fileImporter(isPresented: $choosingPicture, allowedContentTypes: [.image]) { result in
                    do {
                        try Pictures.use(try result.get(), for: game.id)
                        pictureChanged += 1
                    } catch {
                        problem = error.localizedDescription
                    }
                }
            #endif
            if Pictures.exists(game.id) {
                Button("Remove the picture (the game takes a new one)", role: .destructive) {
                    Pictures.remove(game.id)
                    pictureChanged += 1
                }
            }
        }
        GameScreenSection(romset: game.id)
        if let core = CoreLibrary.load(game.id) {
            Section("Core") {
                LabeledContent("Version", value: core.version)
            }
        }
        ControllerMapSection(game: game)
        Section("ROM set") {
            if Storage.rom(game.id) != nil {
                Text("On this device")
                if let folder = cloud.folder {
                    Button(Storage.isFolder(game.id) ? "Copy to iCloud Drive (for your other devices)" : "Put in iCloud Drive (shared with your other devices)") {
                        run { try await Storage.moveToCloud(game.id, folder: folder) }
                    }
                }
                Button("Remove from this device", role: .destructive) {
                    Storage.removeROM(game.id)
                    changed()
                }
            }
            if let item = cloud.items[game.id] {
                Text(item.downloaded ? "In iCloud Drive, downloaded to this device" : "In iCloud Drive, not downloaded to this device")
                if item.downloaded {
                    Button("Remove the download (it stays in iCloud Drive)") {
                        run { try CloudROMs.evict(item.url) }
                    }
                }
                Button("Delete from iCloud Drive (all your devices)", role: .destructive) {
                    run { try await CloudROMs.delete(item.url) }
                }
            }
            if Storage.rom(game.id) == nil && cloud.items[game.id] == nil {
                Text("No ROM set yet: add it or download it above.").foregroundStyle(.secondary)
            }
            if let problem {
                Text(problem).font(.caption).foregroundStyle(.red)
            }
        }
        .disabled(busy)
    }

    private var help: String {
        let setting = game.id == "srallyc" ? "GAME ASSIGNMENTS > LINK TYPE (CAR 1 on one, CAR 2 on the next)"
                                           : "GAME SYSTEM > LINK ID (one MASTER, the others SLAVE) and a different CAR NUMBER"
        return "Cabinets link in a ring: each listens on its port and connects to the next; with two, each one's next is the other. In the game's test mode (F2) set \(setting)."
    }

    private func run(_ action: @escaping () async throws -> Void) {
        busy = true
        problem = nil
        Task {
            do {
                try await action()
                changed()
            } catch {
                problem = error.localizedDescription
            }
            busy = false
        }
    }

    /// Fills in the next cabinet from one found on the network.
    private func pick(_ peer: LinkDiscovery.Peer) {
        busy = true
        problem = nil
        Task {
            if let address = await discovery.address(of: peer, romset: game.id) {
                next = address
                enabled = true
            } else {
                problem = "Could not reach \(peer.name) on the network."
            }
            busy = false
        }
    }

    private func save() {
        let l = LinkSettings(romset: game.id)
        let oldPort = l.port
        l.enabled = enabled
        l.port = max(1, min(65535, port))
        l.next = next.trimmingCharacters(in: .whitespaces)
        l.frameSync = frameSync
        if l.port != oldPort { discovery.advertise() }
    }
}

#if os(tvOS)
/// Takes Back (B, the remote's Back) only while active: an `onExitCommand`
/// left on the view, even with no action, kept Back from leaving the game's
/// page (#177).
private struct SwallowBack: ViewModifier {
    let active: Bool

    func body(content: Content) -> some View {
        if active { content.onExitCommand {} } else { content }
    }
}
#endif
