import Foundation
import UniformTypeIdentifiers

/// The arcade platforms, for the library's filter.
enum Platform: String, CaseIterable, Identifiable {
    case classic2d, model1, model2, model3, lindbergh
    var id: String { rawValue }
    var name: String {
        switch self {
        case .classic2d: "Classic 2D"
        case .model1: "Model 1"
        case .model2: "Model 2"
        case .model3: "Model 3"
        case .lindbergh: "Lindbergh"
        }
    }
}

/// What this release leaves out while it is untested. The code stays: take a
/// game's ROM set (or the headset button) out of here to bring it back.
enum ThisRelease {
    /// Games not listed in the library (their cores, controls and saves stay).
    static let hiddenGames: Set<String> = {
        let sets: Set<String> = ["letsgoju", "segartv"] // Let's Go Jungle!, Sega Race TV: barely tested
        return sets
    }()
    /// The Mac's "Play in Apple Vision Pro" button: not yet played end to end.
    static let headsetButton = false
}

/// The ROM sets Arcade Foundry: Insert Coin knows, one per version of a game. A version is
/// playable when this build has its core and the user has added its ROM set.
struct GameInfo: Identifiable, Hashable {
    let id: String // the MAME ROM set
    let title: String
    let detail: String
    var platform = Platform.model2
    var link = true // the core has link play
    var flight = false // a flight stick and throttle (Star Wars Arcade, Wing War), not a wheel and pedals
    var twin = false // two sticks, each with a shot and a dash (Virtual-On)
    var analogGun = false // Model 3's light guns: no off-screen sense, so the second trigger reloads

    /// Aimed like a gun (the light gun and turret games): the mouse, a
    /// touch, or either stick of a controller.
    var aimsGun: Bool { analogGun || ["vcop", "vcop2", "hotdo", "gunblade", "bel", "rchase2", "hotd4a", "letsgoju"].contains(id) }

    /// The gun has a button besides the trigger: The House of the Dead 4's
    /// grenade (BUTTON3; Controls).
    var gunButton: Bool { id == "hotd4a" }

    /// Gets a crosshair where a controller aims (CrosshairView): the gun
    /// games but Gunblade NY, Behind Enemy Lines and Rail Chase 2, whose
    /// turrets draw their own sight. The game's page can turn it off.
    var showsCrosshair: Bool { aimsGun && !["gunblade", "bel", "rchase2"].contains(id) }

    /// The left stick's forward and back is a second analog control
    /// (afcore_input.stick_y): Sega Ski Super G's swing, Top Skater's slide,
    /// Sega Bass Fishing's rod.
    var stickY: Bool { ["skisuprg", "topskatr"].contains(id) || fishing }

    /// Sega Bass Fishing and Get Bass: the left stick is the rod, the right
    /// trigger the reel, the d-pad the lure stick, X cast and A select.
    var fishing: Bool { ["bassdx", "getbass", "getbassur", "getbassdx"].contains(id) }

    /// A joystick and buttons (the fighting games, and the others played
    /// the same way): X, A, B and Y are buttons 1-4 (punch, kick, guard).
    var fighting: Bool {
        ["doab", "vf2", "fvipers", "vf", "vstriker", "zerogun", "pltkids", "dynamcop", "lastbrnx", "schamp", "dynabb97",
         "vf3", "fvipers2", "spikeout", "spikeofe", "vs299a", "magtruck"].contains(id)
    }

    /// The games this device can run, less `ThisRelease.hiddenGames`. Apple
    /// TV has no Lindbergh games: a Lindbergh core needs a 4 GB address window
    /// (iOS's extended virtual addressing), which tvOS does not give an app.
    static let all: [GameInfo] = {
        #if os(tvOS)
        catalog.filter { $0.platform != .lindbergh }
        #else
        catalog.filter { !ThisRelease.hiddenGames.contains($0.id) }
        #endif
    }()

    private static let catalog: [GameInfo] = [
        GameInfo(id: "daytona93", title: "Daytona USA", detail: "1993", link: false),
        GameInfo(id: "daytona", title: "Daytona USA", detail: "1994, Revision A (link play)"),
        GameInfo(id: "srallyc", title: "Sega Rally Championship", detail: "1995, Revision C (link play)"),
        GameInfo(id: "manxttc", title: "Manx TT Superbike", detail: "1995, Revision C (Twin)"),
        GameInfo(id: "motoraid", title: "Motor Raid", detail: "1997, Twin (Model 2A)", link: false),
        GameInfo(id: "doab", title: "Dead or Alive", detail: "1996, Revision B (Model 2B; X punch, A kick, B hold; two players: a second controller)", link: false),
        GameInfo(id: "lastbrnx", title: "Last Bronx", detail: "1996, Export, Revision A (Model 2B; two players: a second controller)", link: false),
        GameInfo(id: "schamp", title: "Sonic Championship", detail: "1996, USA (Model 2B; two players: a second controller)", link: false),
        GameInfo(id: "dynabb97", title: "Dynamite Baseball 97", detail: "1997, Revision A (Model 2B; X and A the buttons, B swings the bat; two players: a second controller)", link: false),
        GameInfo(id: "waverunr", title: "Wave Runner", detail: "1996, Japan, Revision A (Model 2C)", link: false),
        GameInfo(id: "bel", title: "Behind Enemy Lines", detail: "1997 (Model 2C)", link: false),
        GameInfo(id: "topskatr", title: "Top Skater", detail: "1997, Export, Revision A (Model 2C; the left stick curves and slides)", link: false),
        GameInfo(id: "skisuprg", title: "Sega Ski Super G", detail: "1996 (Model 2C)", link: false),
        GameInfo(id: "segawski", title: "Sega Water Ski", detail: "1997, Japan, Revision A (Model 2C)", link: false),
        GameInfo(id: "desert", title: "Desert Tank", detail: "1994 (Model 2)", link: false),
        GameInfo(id: "skytargt", title: "Sky Target", detail: "1995 (Model 2A; the left stick flies)", link: false, flight: true),
        GameInfo(id: "dynamcop", title: "Dynamite Cop", detail: "1998, Export (Model 2A; two players: a second controller)", link: false),
        GameInfo(id: "vcop2", title: "Virtua Cop 2", detail: "1995 (light gun: mouse or touch)", link: false),
        GameInfo(id: "vcop", title: "Virtua Cop", detail: "1994, Revision B (light gun: mouse or touch)", link: false),
        GameInfo(id: "vf2", title: "Virtua Fighter 2", detail: "1994, Version 2.1 (two players: a second controller)", link: false),
        GameInfo(id: "indy500", title: "INDY 500", detail: "1995, Twin, Revision A (Model 2B)"),
        GameInfo(id: "fvipers", title: "Fighting Vipers", detail: "1995, Revision D (two players: a second controller)", link: false),
        GameInfo(id: "sgt24h", title: "Super GT 24h", detail: "1996, Jaleco (Model 2B)", link: false),
        GameInfo(id: "overrevb", title: "Over Rev", detail: "1997, Jaleco (Model 2B)", link: false),
        GameInfo(id: "stcc", title: "Sega Touring Car Championship", detail: "1996, Model 2C", link: false),
        GameInfo(id: "vstriker", title: "Virtua Striker", detail: "1994, Revision A (Model 2B; two players: a second controller)", link: false),
        GameInfo(id: "zerogun", title: "Zero Gunner", detail: "1997, Export (Model 2B; two players: a second controller)", link: false),
        GameInfo(id: "pltkids", title: "Pilot Kids", detail: "1998, Revision A (Model 2B; two players: a second controller)", link: false),
        GameInfo(id: "rchase2", title: "Rail Chase 2", detail: "1994, Revision A (Model 2B; gun turrets: mouse or touch; two players: a second controller)", link: false),
        GameInfo(id: "gunblade", title: "Gunblade NY", detail: "1995, Revision A (Model 2B; gun turret: mouse or touch)", link: false),
        GameInfo(id: "hotdo", title: "The House of the Dead", detail: "1997, Model 2C (light gun: mouse or touch)", link: false),
        GameInfo(id: "von", title: "Cyber Troopers Virtual-On", detail: "1995, Twin, Export (Model 2B; both sticks of the controller)", link: false, twin: true),
        GameInfo(id: "vr", title: "Virtua Racing", detail: "1992 (link play)", platform: .model1),
        GameInfo(id: "swa", title: "Star Wars Arcade", detail: "1993", platform: .model1, link: false, flight: true),
        GameInfo(id: "vf", title: "Virtua Fighter", detail: "1993 (two players: a second controller)", platform: .model1, link: false),
        GameInfo(id: "wingwar", title: "Wing War", detail: "1994", platform: .model1, link: false, flight: true),
        GameInfo(id: "vf3", title: "Virtua Fighter 3", detail: "1996 (two players: a second controller)", platform: .model3, link: false),
        GameInfo(id: "scud", title: "Scud Race", detail: "1996", platform: .model3, link: false),
        GameInfo(id: "scudplus", title: "Scud Race", detail: "1997, Scud Race Plus", platform: .model3, link: false),
        GameInfo(id: "daytona2", title: "Daytona USA 2", detail: "1998, Battle on the Edge", platform: .model3, link: false),
        GameInfo(id: "dayto2pe", title: "Daytona USA 2", detail: "1998, Power Edition", platform: .model3, link: false),
        GameInfo(id: "srally2", title: "Sega Rally 2", detail: "1998", platform: .model3, link: false),
        GameInfo(id: "lemans24", title: "Le Mans 24", detail: "1997", platform: .model3, link: false),
        GameInfo(id: "dirtdvls", title: "Dirt Devils", detail: "1998", platform: .model3, link: false),
        GameInfo(id: "harley", title: "Harley-Davidson & L.A. Riders", detail: "1997 (two coins per credit)", platform: .model3, link: false),
        GameInfo(id: "ecau", title: "Emergency Call Ambulance", detail: "1999", platform: .model3, link: false),
        GameInfo(id: "fvipers2", title: "Fighting Vipers 2", detail: "1998 (two players: a second controller)", platform: .model3, link: false),
        GameInfo(id: "von2", title: "Cyber Troopers Virtual-On Oratorio Tangram", detail: "1998 (both sticks of the controller; two coins per credit)", platform: .model3, link: false, twin: true),
        GameInfo(id: "spikeout", title: "Spikeout", detail: "1998", platform: .model3, link: false),
        GameInfo(id: "spikeofe", title: "Spikeout", detail: "1999, Final Edition", platform: .model3, link: false),
        GameInfo(id: "vs299a", title: "Virtua Striker 2 '99", detail: "1999 (two players: a second controller)", platform: .model3, link: false),
        GameInfo(id: "swtrilgy", title: "Star Wars Trilogy Arcade", detail: "1998 (the left stick flies)", platform: .model3, link: false, flight: true),
        GameInfo(id: "lostwsga", title: "The Lost World", detail: "1997 (light gun: mouse or touch)", platform: .model3, link: false, analogGun: true),
        GameInfo(id: "lamachin", title: "L.A. Machineguns", detail: "1998 (light gun: mouse or touch)", platform: .model3, link: false, analogGun: true),
        GameInfo(id: "oceanhuna", title: "The Ocean Hunter", detail: "1998 (light gun: mouse or touch; two coins per credit)", platform: .model3, link: false, analogGun: true),
        GameInfo(id: "bassdx", title: "Sega Bass Fishing", detail: "1998, Deluxe, USA (the left stick is the rod, the right trigger reels, X casts)", platform: .model3, link: false),
        GameInfo(id: "getbass", title: "Get Bass", detail: "1998, Japan, Standard (the left stick is the rod, the right trigger reels, X casts)", platform: .model3, link: false),
        GameInfo(id: "getbassur", title: "Get Bass", detail: "1998, Japan, Upright (the left stick is the rod, the right trigger reels, X casts)", platform: .model3, link: false),
        GameInfo(id: "getbassdx", title: "Get Bass", detail: "1998, Japan, Deluxe (needs bassdx.zip beside it; the left stick is the rod, the right trigger reels, X casts; two coins per credit)", platform: .model3, link: false),
        GameInfo(id: "magtruck", title: "Magical Truck Adventure", detail: "1998, Japan (pump the lever: the stick up and down; X the pedal; two players: a second controller)", platform: .model3, link: false),
        GameInfo(id: "hotd4a", title: "The House of the Dead 4", detail: "2005, Revision A (light gun: mouse or touch; middle-click, G, X or three fingers throw a grenade; the game folder that holds disk0)", platform: .lindbergh, link: false),
        GameInfo(id: "letsgoju", title: "Let's Go Jungle!", detail: "2006, Lost on the Island of Spice (light gun: mouse or touch, hold to fire; the game folder that holds disk0)", platform: .lindbergh, link: false),
        GameInfo(id: "segartv", title: "Sega Race TV", detail: "2008 (wheel and pedals; the game folder that holds disk0)", platform: .lindbergh, link: false),
        GameInfo(id: "outr2sdx", title: "OutRun 2 SP SDX", detail: "2006 (wheel and pedals; the game folder that holds disk0)", platform: .lindbergh, link: false),
        GameInfo(id: "outrun", title: "Out Run", detail: "1986, Revision B (Out Run board; the shift buttons move the gear lever: low and high)", platform: .classic2d, link: false),
        GameInfo(id: "shangon", title: "Super Hang-On", detail: "1987, sit-down/upright (Out Run board; the right shoulder is the turbo)", platform: .classic2d, link: false),
        GameInfo(id: "aburner2", title: "After Burner II", detail: "1987 (X Board; the left stick flies, the right trigger is the throttle)", platform: .classic2d, link: false, flight: true),
        GameInfo(id: "thndrbld", title: "Thunder Blade", detail: "1987, upright (X Board; the left stick flies, the right trigger is the throttle)", platform: .classic2d, link: false, flight: true),
    ]
}

/// One card in the library: a game on one platform, with its versions
/// (Daytona USA on Model 2: daytona93 and daytona).
struct ArcadeGame: Identifiable, Hashable {
    let title: String
    let platform: Platform
    let versions: [GameInfo]
    var id: String { "\(platform.rawValue)/\(title)" }

    /// GameInfo.all grouped by title and platform, in catalog order.
    static let all: [ArcadeGame] = {
        var order: [String] = [], groups: [String: [GameInfo]] = [:]
        for game in GameInfo.all {
            let key = "\(game.platform.rawValue)/\(game.title)"
            if groups[key] == nil { order.append(key) }
            groups[key, default: []].append(game)
        }
        return order.map { key in
            let v = groups[key]!
            return ArcadeGame(title: v[0].title, platform: v[0].platform, versions: v)
        }
    }()

    static func with(id: String) -> ArcadeGame? { all.first { $0.id == id } }

    /// The release year of its earliest version (each detail starts with it).
    var year: Int { versions.compactMap { Int($0.detail.prefix(4)) }.min() ?? 9999 }

    /// The title for A–Z: "The House of the Dead" under H.
    private var sortTitle: String { title.hasPrefix("The ") ? String(title.dropFirst(4)) : title }

    /// `games` in the library's order. Platform is Model 1, Model 2, Model 3,
    /// Lindbergh (their release order), each by year; Recently played puts
    /// the games never played after the others, A–Z.
    static func sorted(_ games: [ArcadeGame], by sort: LibrarySort) -> [ArcadeGame] {
        let byTitle: (ArcadeGame, ArcadeGame) -> Bool = {
            $0.sortTitle.localizedStandardCompare($1.sortTitle) == .orderedAscending
        }
        let byYear: (ArcadeGame, ArcadeGame) -> Bool = { $0.year != $1.year ? $0.year < $1.year : byTitle($0, $1) }
        switch sort {
        case .title:
            return games.sorted(by: byTitle)
        case .year:
            return games.sorted(by: byYear)
        case .platform:
            let rank = { (g: ArcadeGame) in Platform.allCases.firstIndex(of: g.platform) ?? 0 }
            return games.sorted { rank($0) != rank($1) ? rank($0) < rank($1) : byYear($0, $1) }
        case .recent:
            let played = LastPlayed.all()
            let last = { (g: ArcadeGame) in g.versions.compactMap { played[$0.id] }.max() }
            return games.sorted {
                switch (last($0), last($1)) {
                case let (a?, b?): return a != b ? a > b : byTitle($0, $1)
                case (.some, nil): return true
                case (nil, .some): return false
                case (nil, nil): return byTitle($0, $1)
                }
            }
        }
    }
}

/// The Lindbergh games took MAME's set names (2026-10-28): a device's game
/// folders, saves, pictures and settings under the old names move to the new
/// ones, once, at launch. iCloud Drive files and ROM server files under the
/// old names are still found (Storage.otherNames).
enum SetRenames {
    static let renamed = ["hotd4": "hotd4a", "lgj": "letsgoju", "or2sp": "outr2sdx"]

    static func migrate() {
        let fm = FileManager.default
        for (old, new) in renamed {
            let moves = [(Storage.roms.appendingPathComponent(old, isDirectory: true), Storage.roms.appendingPathComponent(new, isDirectory: true)),
                         (Storage.saves(old), Storage.saves(new)),
                         (Pictures.url(old), Pictures.url(new))]
            for (from, to) in moves where fm.fileExists(atPath: from.path) && !fm.fileExists(atPath: to.path) {
                try? fm.moveItem(at: from, to: to)
            }
        }
        // Per-game settings: "<kind>.<romset>[.<more>]" (screenLook., buttons., replay., link., vrView.)
        let defaults = UserDefaults.standard
        for (key, value) in defaults.dictionaryRepresentation() {
            var parts = key.components(separatedBy: ".")
            guard parts.count >= 2, let new = renamed[parts[1]] else { continue }
            parts[1] = new
            let newKey = parts.joined(separator: ".")
            if defaults.object(forKey: newKey) == nil { defaults.set(value, forKey: newKey) }
            defaults.removeObject(forKey: key)
        }
        if var played = defaults.dictionary(forKey: SettingKey.lastPlayed), played.keys.contains(where: { renamed[$0] != nil }) {
            for (old, new) in renamed { if let t = played.removeValue(forKey: old), played[new] == nil { played[new] = t } }
            defaults.set(played, forKey: SettingKey.lastPlayed)
        }
    }
}

/// Where the app keeps ROM sets and each game's saved settings:
/// Application Support/Arcade Foundry/{ROMs,Saves/<romset>}.
enum Storage {
    static let root: URL = {
        #if os(tvOS)
        // tvOS apps may only keep files in Caches (the system can clear it:
        // the ROM sets download again from the ROM server).
        let base = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
        #else
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        #endif
        return base.appendingPathComponent("Arcade Foundry", isDirectory: true)
    }()
    static var roms: URL { root.appendingPathComponent("ROMs", isDirectory: true) }
    static func saves(_ romset: String) -> URL { root.appendingPathComponent("Saves/\(romset)", isDirectory: true) }

    static let romExtensions = ["zip", "7z"]
    static var romTypes: [UTType] {
        [UTType.zip, .folder] + [UTType(filenameExtension: "7z"), UTType(filenameExtension: GamePackage.ext)].compactMap { $0 }
    }

    /// Lindbergh games are a folder, not an archive: the game's disk as the
    /// cabinet mounts it (the folder that holds disk0), named after its set
    /// ("hotd4a"). In iCloud Drive and on the ROM server they are one file,
    /// <set>.tar (GamePackage), unpacked into the folder before play.
    static func isFolder(_ romset: String) -> Bool {
        GameInfo.all.first { $0.id == romset }?.platform == .lindbergh
    }

    /// The ROM set the user added for `romset`, if any.
    static func rom(_ romset: String) -> URL? {
        if isFolder(romset) {
            let url = roms.appendingPathComponent(romset, isDirectory: true)
            return FileManager.default.fileExists(atPath: url.appendingPathComponent("disk0").path) ? url : nil
        }
        // Over Rev's split .zip lacks its parent's files: the .7z (the whole set) first.
        for ext in romset == "overrevb" ? Array(romExtensions.reversed()) : romExtensions {
            let url = roms.appendingPathComponent("\(romset).\(ext)")
            if FileManager.default.fileExists(atPath: url.path) { return url }
        }
        return nil
    }

    /// Other names a set's file may have, and the app's set they are
    /// (scripts/rom_index.py's OTHER_NAMES): MAME parents that hold a clone's
    /// set (hotd.7z with The House of the Dead's original set, hotdo; doa.zip
    /// with Dead or Alive's Revision B, doab, in older romsets) and the names
    /// the app gave the Lindbergh games before it took MAME's (or2sp.tar is
    /// outr2sdx; see SetRenames).
    static let otherNames = ["hotd": "hotdo", "doa": "doab", "or2sp": "outr2sdx", "lgj": "letsgoju", "hotd4": "hotd4a"]

    /// The app's set and the other names its files may have.
    static func names(_ romset: String) -> [String] {
        [romset] + otherNames.filter { $0.value == romset }.map(\.key).sorted()
    }

    /// MAME device sets a game's set needs beside it (Wing War's I/O board
    /// BIOS, model1io2.zip), added like a game's ROM set.
    static let devices = ["wingwar": ["model1io2"]]

    /// The set a ROM file is for ("srallyc.7z" is srallyc), if Arcade Foundry: Insert Coin
    /// knows it (a game's, or a device set one needs).
    static func romset(of url: URL) -> String? {
        if url.hasDirectoryPath {
            var set = url.lastPathComponent.lowercased()
            if let app = otherNames[set] { set = app }
            return isFolder(set) ? set : nil
        }
        let ext = url.pathExtension.lowercased()
        var set = url.deletingPathExtension().lastPathComponent.lowercased()
        if let app = otherNames[set] { set = app }
        if ext == GamePackage.ext { return isFolder(set) ? set : nil }
        guard romExtensions.contains(ext),
              GameInfo.all.contains(where: { $0.id == set }) || devices.values.contains(where: { $0.contains(set) }) else { return nil }
        return set
    }

    /// Adds a ROM set the user picked, named after its set ("srallyc.7z"),
    /// replacing an older one: into iCloud Drive when `cloud` is its folder
    /// (the user's other devices get it too), otherwise into this device's
    /// ROM folder.
    static func addROM(from source: URL, cloud: URL?) async throws -> String {
        guard let set = romset(of: source) else {
            throw CoreError.failed("\(source.lastPathComponent) is not a ROM set Arcade Foundry: Insert Coin knows (name it after its MAME set: \(GameInfo.all.map(\.id).joined(separator: ", ")), .zip or .7z; a Lindbergh game is its folder or <set>.tar, named after its set)")
        }
        let ext = source.pathExtension.lowercased()
        if isFolder(set) && source.hasDirectoryPath {
            // Played here, and packed into iCloud Drive for the other devices.
            try await Task.detached(priority: .userInitiated) { try installFolder(source, set: set) }.value
            if let cloud { try await packToCloud(set, into: cloud) }
            return set
        }
        if isFolder(set) && cloud == nil {
            try await Task.detached(priority: .userInitiated) { try GamePackage.unpack(file: source, set: set) }.value
            return set
        }
        if let cloud {
            try await CloudROMs.copy(source, set: set, ext: ext, into: cloud)
        } else {
            try install(source, set: set, ext: ext, move: false)
        }
        return set
    }

    /// Adds the ROM sets the user picked (Add ROM); what to tell them.
    static func addROMs(_ urls: [URL], cloud: URL?) async -> String? {
        var added: [String] = [], problem: String?
        for url in urls {
            do { added.append(try await addROM(from: url, cloud: cloud)) } catch { problem = error.localizedDescription }
        }
        guard !added.isEmpty else { return problem }
        return "Added \(added.joined(separator: ", "))" + (cloud != nil ? " to iCloud Drive." : ".")
    }

    /// The ROM set to play `romset` with, on this device: the local one, or
    /// the one in iCloud Drive, downloaded first if needed.
    @MainActor static func ready(_ romset: String) async throws -> URL {
        let url: URL
        if let local = rom(romset) {
            url = local
        } else if let item = CloudROMs.shared.items[romset], isFolder(romset) {
            // <set>.tar: unpacked here, then only the game folder is kept on this device.
            let tar = try await CloudROMs.download(item.url)
            try await Task.detached(priority: .userInitiated) { try GamePackage.unpack(file: tar, set: romset) }.value
            try? CloudROMs.evict(item.url)
            guard let local = rom(romset) else { throw CoreError.failed("\(romset).\(GamePackage.ext) did not unpack") }
            url = local
        } else if let item = CloudROMs.shared.items[romset] {
            url = try await CloudROMs.download(item.url)
        } else {
            throw CoreError.failed("There is no ROM set for \(romset) on this device or in iCloud Drive.")
        }
        for device in devices[romset] ?? [] { try await place(device, beside: url) }
        return url
    }

    /// A device set where the core looks for it: beside the game's set.
    @MainActor private static func place(_ device: String, beside url: URL) async throws {
        let dir = url.deletingLastPathComponent()
        let fm = FileManager.default
        if romExtensions.contains(where: { fm.fileExists(atPath: dir.appendingPathComponent("\(device).\($0)").path) }) { return }
        var source = rom(device)
        if source == nil, let item = CloudROMs.shared.items[device] { source = try await CloudROMs.download(item.url) }
        guard let source else {
            throw CoreError.failed("This game also needs MAME's \(device) set (its I/O board's BIOS): add \(device).zip like a ROM set.")
        }
        try fm.copyItem(at: source, to: dir.appendingPathComponent(source.lastPathComponent))
    }

    /// Moves this device's ROM set for `romset` into iCloud Drive. A game
    /// folder is packed in as <set>.tar and stays here too (unpacking it
    /// again takes minutes).
    static func moveToCloud(_ romset: String, folder: URL) async throws {
        if isFolder(romset) {
            if rom(romset) != nil { try await packToCloud(romset, into: folder) }
            return
        }
        guard let url = rom(romset) else { return }
        try await CloudROMs.copy(url, set: romset, ext: url.pathExtension.lowercased(), into: folder)
        removeROM(romset)
    }

    /// Packs this device's game folder into iCloud Drive as <set>.tar.
    static func packToCloud(_ romset: String, into cloud: URL) async throws {
        guard let folder = rom(romset) else { return }
        let tmp = FileManager.default.temporaryDirectory.appendingPathComponent("\(romset).\(GamePackage.ext)")
        defer { try? FileManager.default.removeItem(at: tmp) }
        try await Task.detached(priority: .userInitiated) { try GamePackage.pack(folder, set: romset, to: tmp) }.value
        try await CloudROMs.copy(tmp, set: romset, ext: GamePackage.ext, into: cloud)
    }

    static func install(_ file: URL, set: String, ext: String, move: Bool) throws {
        let fm = FileManager.default
        try fm.createDirectory(at: roms, withIntermediateDirectories: true)
        for old in romExtensions {
            try? fm.removeItem(at: roms.appendingPathComponent("\(set).\(old)"))
        }
        let dst = roms.appendingPathComponent("\(set).\(ext)")
        let scoped = file.startAccessingSecurityScopedResource()
        defer { if scoped { file.stopAccessingSecurityScopedResource() } }
        if move { try fm.moveItem(at: file, to: dst) } else { try fm.copyItem(at: file, to: dst) }
    }

    /// Copies a Lindbergh game folder into the ROM folder as <set>/
    /// (a clone, not a copy, on the same APFS volume).
    static func installFolder(_ source: URL, set: String) throws {
        let fm = FileManager.default
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        guard fm.fileExists(atPath: source.appendingPathComponent("disk0").path) else {
            throw CoreError.failed("\(source.lastPathComponent) has no disk0 folder: pick the game folder that holds disk0")
        }
        try fm.createDirectory(at: roms, withIntermediateDirectories: true)
        let dst = roms.appendingPathComponent(set, isDirectory: true)
        try? fm.removeItem(at: dst)
        try fm.copyItem(at: source, to: dst)
    }

    static func removeROM(_ romset: String) {
        if isFolder(romset) {
            try? FileManager.default.removeItem(at: roms.appendingPathComponent(romset, isDirectory: true))
            return
        }
        for ext in romExtensions {
            try? FileManager.default.removeItem(at: roms.appendingPathComponent("\(romset).\(ext)"))
        }
    }
}

/// Downloads ROM sets from the user's own server: <server>/<romset>.zip
/// (or .7z; a Lindbergh game's <set>.tar, unpacked as it arrives), once
/// per device, into the device's ROM folder. Any web
/// server that serves the files works, e.g. on the machine with the ROMs:
/// python3 -m http.server 8000 --directory ~/path/to/roms
enum RomServer {
    /// Gives up on a connection that stalls (a server that is gone, or
    /// Local Network access not given to the app on iOS) instead of
    /// waiting for ever.
    static let session: URLSession = {
        let c = URLSessionConfiguration.default
        c.timeoutIntervalForRequest = 30
        c.waitsForConnectivity = false
        return URLSession(configuration: c)
    }()
    /// The server did not answer: the other sets would wait as long.
    struct Unreachable: LocalizedError {
        let errorDescription: String?
    }

    @MainActor private static var running: [String: Task<Void, Error>] = [:]

    /// Downloads `romset` (and its device sets). A game's page asking for a
    /// set the library is already downloading waits for that download.
    @MainActor static func download(_ romset: String, from server: String) async throws {
        if let task = running[romset] { return try await task.value }
        let task = Task { try await fetch(romset, from: server) }
        running[romset] = task
        defer {
            running[romset] = nil
            Downloads.shared.set(romset, nil)
        }
        try await task.value
    }

    private static func fetch(_ romset: String, from server: String) async throws {
        guard var base = URL(string: server.trimmingCharacters(in: .whitespaces)), base.scheme != nil else {
            throw CoreError.failed("The ROM server address is not a URL (for example http://192.168.1.10:8000/roms)")
        }
        if base.path.hasSuffix("/") { base.deleteLastPathComponent() }
        do {
            var lastError: Error = CoreError.failed("not found")
            let names = Storage.names(romset) // hotd.7z for hotdo, or2sp.tar for outr2sdx
            if Storage.isFolder(romset) {
                for name in names {
                    do {
                        try await PackageDownload.run(base.appendingPathComponent("\(name).\(GamePackage.ext)"), set: romset)
                        return
                    } catch let error as URLError where unreachable(error) {
                        throw error
                    } catch {
                        lastError = error
                    }
                }
                throw lastError
            }
            for (name, ext) in names.flatMap({ name in Storage.romExtensions.map { (name, $0) } }) {
                let url = base.appendingPathComponent("\(name).\(ext)")
                do {
                    let (file, response) = try await session.download(from: url, delegate: DownloadProgress(set: romset))
                    guard (response as? HTTPURLResponse)?.statusCode == 200 else {
                        try? FileManager.default.removeItem(at: file)
                        lastError = CoreError.failed("\(url.absoluteString): not on the server")
                        continue
                    }
                    try Storage.install(file, set: romset, ext: ext, move: true)
                    for device in Storage.devices[romset] ?? [] where Storage.rom(device) == nil {
                        try await download(device, from: server) // its device sets too
                    }
                    return
                } catch let error as URLError where unreachable(error) {
                    throw error // the next name would wait as long
                } catch {
                    lastError = error
                }
            }
            throw lastError
        } catch let error as URLError where unreachable(error) {
            throw Unreachable(errorDescription: "Could not reach the ROM server at \(base.absoluteString) (\(error.localizedDescription))." + localNetworkHint)
        }
    }

    private static func unreachable(_ error: URLError) -> Bool {
        [.timedOut, .cannotConnectToHost, .cannotFindHost, .notConnectedToInternet,
         .networkConnectionLost, .dnsLookupFailed].contains(error.code)
    }

    private static var localNetworkHint: String {
        #if os(iOS) || os(tvOS)
        " Check that the server is running, and that Arcade Foundry: Insert Coin is on in Settings > Privacy & Security > Local Network."
        #else
        " Check that the server is running and the address is right."
        #endif
    }
}

/// Shows a .zip or .7z download's progress (the library's "Downloading… 42%").
private final class DownloadProgress: NSObject, URLSessionTaskDelegate, @unchecked Sendable {
    private let set: String
    private var shown = -1.0
    private var observation: NSKeyValueObservation?

    init(set: String) { self.set = set }

    func urlSession(_ session: URLSession, didCreateTask task: URLSessionTask) {
        observation = task.progress.observe(\.fractionCompleted) { [weak self] progress, _ in
            guard let self, progress.totalUnitCount > 0 else { return }
            let f = progress.fractionCompleted
            guard f - self.shown >= 0.01 else { return }
            self.shown = f
            let set = self.set
            Task { @MainActor in Downloads.shared.set(set, f) }
        }
    }
}
