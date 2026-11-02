import Foundation

/// A game's history for its page, from history/<romset>.md in the app (one
/// file per game, named after any of its versions):
///
///     # Title
///
///     Developer · Year · Board
///
///     Paragraphs, blank lines between them.
///
///     ## Read more
///
///     - [Label](https://...)
struct GameHistory {
    let facts: String
    let paragraphs: [String]
    let links: [(label: String, url: URL)]

    static func load(_ game: ArcadeGame) -> GameHistory? {
        for version in game.versions {
            if let url = Bundle.main.url(forResource: version.id, withExtension: "md", subdirectory: "history"),
               let text = try? String(contentsOf: url, encoding: .utf8) {
                return parse(text)
            }
        }
        return nil
    }

    static func parse(_ text: String) -> GameHistory? {
        var blocks: [String] = [], links: [(label: String, url: URL)] = [], current: [String] = []
        var inLinks = false
        let flush = {
            if !current.isEmpty { blocks.append(current.joined(separator: " ")) }
            current = []
        }
        for raw in text.components(separatedBy: .newlines) {
            let line = raw.trimmingCharacters(in: .whitespaces)
            if line.isEmpty {
                flush()
            } else if line.hasPrefix("## ") {
                flush()
                inLinks = true
            } else if line.hasPrefix("# ") {
                continue
            } else if inLinks {
                // - [Label](url)
                guard let open = line.firstIndex(of: "["), let close = line.range(of: "]("),
                      line.hasSuffix(")"), open < close.lowerBound,
                      let url = URL(string: String(line[close.upperBound..<line.index(before: line.endIndex)]))
                else { continue }
                links.append((label: String(line[line.index(after: open)..<close.lowerBound]), url: url))
            } else {
                current.append(line)
            }
        }
        flush()
        guard let facts = blocks.first else { return nil }
        return GameHistory(facts: facts, paragraphs: Array(blocks.dropFirst()), links: links)
    }
}
