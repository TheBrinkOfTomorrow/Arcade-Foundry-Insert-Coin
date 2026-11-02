import SwiftUI

/// App-wide settings (UserDefaults keys, used with @AppStorage).
enum SettingKey {
    static let romServer = "romServer" // String: the user's ROM server URL
    static let volume = "volume"       // Double 0-1
    static let smooth = "smoothPixels" // Bool: linear filtering of the screen (before screenLook; its default)
    static let screenLook = "screenLook" // String: a ScreenLook, every game's screen settings; "screenLook.<romset>" a game's own
    static let addToCloud = "addToICloud" // Bool: Add ROM puts the set in iCloud Drive (default on)
    static let platformFilter = "platformFilter" // String: the library's platform ("all" or a Platform)
    static let playableOnly = "playableOnly" // Bool: the library shows only the games that can be played now
    static let libraryList = "libraryList" // Bool: the library as a list of rows instead of cards
    static let coreThread = "coreThread" // Bool: the game runs on its own thread (default on; this device only)
    static let screen3D = "screen3D" // String: a Screen3D, the stereo picture on this screen (default off)
    static let librarySort = "librarySort" // String: a LibrarySort, the library's order
    static let lastPlayed = "lastPlayed" // [String: Double]: when each romset was last started (seconds since 1970)
    static let appearance = "appearance" // String: an Appearance (default dark)
    static let recordReplay = "recordReplay" // Bool: every game records a replay (default off; a game's page can differ, ReplayRecorder.key)
    static let showSpeed = "showSpeed" // Bool: iPhone, iPad and Apple TV show the speed over the game (default off; the Mac shows it in the window title)
    /// Bool: a gun game shows its crosshair (default on; GameInfo.showsCrosshair)
    static func crosshair(_ romset: String) -> String { "crosshair.\(romset)" }
}

/// The app's light or dark look. Dark unless the user picks another.
enum Appearance: String, CaseIterable, Identifiable {
    case dark, light, system
    var id: String { rawValue }

    var name: String {
        switch self {
        case .dark: "Dark"
        case .light: "Light"
        case .system: "Match the system"
        }
    }

    var colorScheme: ColorScheme? {
        switch self {
        case .dark: .dark
        case .light: .light
        case .system: nil
        }
    }
}

/// Gives a window's content the chosen Appearance.
struct AppAppearance: ViewModifier {
    @AppStorage(SettingKey.appearance) private var appearance = Appearance.dark

    func body(content: Content) -> some View {
        content.preferredColorScheme(appearance.colorScheme)
        #if os(tvOS)
            .toggleStyle(TVRowToggleStyle())
            .buttonStyle(TVFocusButtonStyle())
        #endif
    }
}

#if os(tvOS)
/// Apple TV: a switch row as a button whose text turns dark on the white
/// focus highlight. The system Toggle kept its label white there in Dark
/// appearance, so the focused row couldn't be read (#261).
struct TVRowToggleStyle: ToggleStyle {
    func makeBody(configuration: Configuration) -> some View {
        Button { configuration.isOn.toggle() } label: {
            TVRowToggleLabel(configuration: configuration)
        }
        .buttonStyle(.automatic)
        .accessibilityValue(configuration.isOn ? "On" : "Off")
    }
}

/// Apple TV: every button without a style of its own stays the system
/// button, but its label turns dark on the white focus highlight, which the
/// system left white in Dark appearance (#261: Size, the cabinet buttons).
/// Buttons with their own style (cards, the pause menu) are unchanged.
struct TVFocusButtonStyle: PrimitiveButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        Button(role: configuration.role, action: configuration.trigger) {
            TVFocusLabel { configuration.label }
        }
        .buttonStyle(.automatic) // not this style again
    }
}

private struct TVFocusLabel<Content: View>: View {
    @ViewBuilder let content: Content
    @Environment(\.isFocused) private var focused

    var body: some View {
        if focused { content.foregroundStyle(Color.black) } else { content }
    }
}

private struct TVRowToggleLabel: View {
    let configuration: ToggleStyleConfiguration
    @Environment(\.isFocused) private var focused

    var body: some View {
        HStack {
            configuration.label
                .foregroundStyle(focused ? Color.black : Color.primary)
            Spacer()
            Text(configuration.isOn ? "On" : "Off")
                .foregroundStyle(focused ? Color.black.opacity(0.6) : Color.secondary)
        }
    }
}
#endif

extension View {
    func appAppearance() -> some View { modifier(AppAppearance()) }
}

/// The library's order.
enum LibrarySort: String, CaseIterable, Identifiable {
    case title, year, platform, recent
    var id: String { rawValue }

    var name: String {
        switch self {
        case .title: "A–Z"
        case .year: "Year"
        case .platform: "Platform"
        case .recent: "Recently played"
        }
    }
}

/// When each game was last started on this device, for the library's
/// Recently played order.
enum LastPlayed {
    static func mark(_ romset: String) {
        var all = UserDefaults.standard.dictionary(forKey: SettingKey.lastPlayed) as? [String: Double] ?? [:]
        all[romset] = Date().timeIntervalSince1970
        UserDefaults.standard.set(all, forKey: SettingKey.lastPlayed)
    }

    static func all() -> [String: Double] {
        UserDefaults.standard.dictionary(forKey: SettingKey.lastPlayed) as? [String: Double] ?? [:]
    }
}

/// Link play for one game (Daytona USA Revision A, Sega Rally): this
/// cabinet's port and the next cabinet's address, as in the recomp's app.
struct LinkSettings {
    let romset: String
    private var d: UserDefaults { .standard }
    private func key(_ k: String) -> String { "link.\(romset).\(k)" }

    var enabled: Bool {
        get { d.bool(forKey: key("enabled")) }
        nonmutating set { d.set(newValue, forKey: key("enabled")) }
    }
    var port: Int {
        get { d.object(forKey: key("port")) as? Int ?? 15112 }
        nonmutating set { d.set(newValue, forKey: key("port")) }
    }
    var next: String {
        get { d.string(forKey: key("next")) ?? "" }
        nonmutating set { d.set(newValue, forKey: key("next")) }
    }
    var frameSync: Bool {
        get { d.bool(forKey: key("frameSync")) }
        nonmutating set { d.set(newValue, forKey: key("frameSync")) }
    }
}
