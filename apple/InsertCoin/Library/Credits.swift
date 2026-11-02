import SwiftUI

/// The credits, from CREDITS.md in the app (the same file as the repository's
/// and the Linux app's):
///
///     # Credits
///
///     Opening paragraph.
///
///     ## Section
///
///     A paragraph, or
///     - **Name**: what they made, with [links](https://...)
struct Credits {
    struct Section: Identifiable {
        let id: Int
        let title: String
        /// Paragraphs and list items, as Markdown (bold and links).
        let lines: [String]
    }

    let opening: [String]
    let sections: [Section]

    static func load() -> Credits? {
        guard let url = Bundle.main.url(forResource: "CREDITS", withExtension: "md"),
              let text = try? String(contentsOf: url, encoding: .utf8)
        else { return nil }
        return parse(text)
    }

    static func parse(_ text: String) -> Credits {
        var opening: [String] = [], sections: [Section] = []
        var title: String?, lines: [String] = [], current: [String] = []
        let flush = {
            if !current.isEmpty { lines.append(current.joined(separator: " ")) }
            current = []
        }
        let close = {
            flush()
            if let title { sections.append(Section(id: sections.count, title: title, lines: lines)) }
            else { opening = lines }
            lines = []
        }
        for raw in text.components(separatedBy: .newlines) {
            let line = raw.trimmingCharacters(in: .whitespaces)
            if line.isEmpty {
                flush()
            } else if line.hasPrefix("## ") {
                close()
                title = String(line.dropFirst(3))
            } else if line.hasPrefix("# ") {
                continue
            } else if line.hasPrefix("- ") {
                flush()
                current = [String(line.dropFirst(2))]
                flush()
            } else {
                current.append(line)
            }
        }
        close()
        return Credits(opening: opening, sections: sections)
    }
}

/// The credits page: Settings > About > Credits on iPhone, iPad and Apple TV,
/// and the About window on the Mac.
struct CreditsView: View {
    private let credits = Credits.load()

    var body: some View {
        Form {
            if let credits {
                Section {
                    ForEach(credits.opening, id: \.self) { CreditLine(markdown: $0) }
                }
                ForEach(credits.sections) { section in
                    Section(section.title) {
                        ForEach(section.lines, id: \.self) { CreditLine(markdown: $0) }
                    }
                }
            } else {
                Text("The credits are missing from this build.").foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .navigationTitle("Credits")
    }
}

/// One paragraph or list item. Apple TV can't open links, so there it is
/// plain text that can take focus (to scroll the page).
private struct CreditLine: View {
    let markdown: String

    var body: some View {
        #if os(tvOS)
        Text(text).focusable()
        #else
        Text(text)
        #endif
    }

    private var text: AttributedString {
        (try? AttributedString(markdown: markdown)) ?? AttributedString(markdown)
    }
}

#if os(macOS)
/// The Mac's About window (the app menu's About item): the logo, the version
/// and the credits.
struct AboutView: View {
    static let windowID = "about"

    var body: some View {
        VStack(spacing: 0) {
            Image("Logo")
                .resizable()
                .scaledToFit()
                .frame(maxHeight: 120)
                .padding(20)
                .frame(maxWidth: .infinity)
                .background(Color("LaunchBackground"))
            Text("Version \(AppInfo.version) · core interface \(CoreLibrary.interfaceVersion)")
                .font(.caption).foregroundStyle(.secondary)
                .padding(.top, 10)
            CreditsView()
        }
        .frame(width: 480, height: 600)
    }
}

/// The app menu's About item, opening AboutView.
struct AboutCommand: View {
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        Button("About Arcade Foundry: Insert Coin") { openWindow(id: AboutView.windowID) }
    }
}

/// Settings > About on the Mac: the credits are in the About window.
struct AboutCreditsButton: View {
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        LabeledContent("Credits") {
            Button("Show") { openWindow(id: AboutView.windowID) }
        }
    }
}
#endif

enum AppInfo {
    /// "1.0.1": the version. The build number (CFBundleVersion) is left out:
    /// it only tells App Store uploads of one version apart, and these builds are sideloaded.
    static var version: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
    }
}
