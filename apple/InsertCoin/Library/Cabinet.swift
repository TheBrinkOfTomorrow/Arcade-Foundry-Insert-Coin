import CloudKit
import Foundation
import Network
#if os(iOS) || os(tvOS)
import UIKit
#endif

/// This device as an arcade cabinet: an ID of its own and a name the user
/// picks. Each game's saved settings (the board's EEPROM and backup RAM,
/// which hold the cabinet's link role as well as its records) are backed up
/// to iCloud under this cabinet, never shared with the others, so linked
/// cabinets keep their roles. On iOS and tvOS the ID lives in the Keychain,
/// which outlives Caches being cleared and usually the app being deleted;
/// on macOS in the app's preferences.
enum Cabinet {
    // The Keychain item's name, not the bundle ID: kept as it is for every
    // build, or devices would lose their cabinet ID.
    private static let service = "com.thebrinkoftomorrow.arcadefoundry.insertcoin.cabinet"
    private static let nameKey = "cabinetName"

    /// Whether this launch made a new ID (a new device, or one that lost
    /// its own): the app then offers to continue as a cabinet in iCloud.
    private(set) static var isNew = false

    static var id: String {
        if let id = stored() { return id }
        let id = UUID().uuidString
        store(id)
        isNew = true
        return id
    }

    static var name: String {
        get { UserDefaults.standard.string(forKey: nameKey) ?? model }
        set { UserDefaults.standard.set(newValue, forKey: nameKey) }
    }

    /// The kind of device, the default name.
    static var model: String {
        #if os(macOS)
        return Host.current().localizedName ?? "Mac"
        #elseif os(tvOS)
        return "Apple TV"
        #else
        return UIDevice.current.model // "iPhone", "iPad"
        #endif
    }

    /// Takes a new ID of its own: leaves the cabinet it shared with another
    /// device (after a transfer), keeping its name and saves.
    static func renew() {
        store(UUID().uuidString)
        isNew = false
    }

    /// Becomes another cabinet (one this device replaces): its ID and name.
    static func adopt(id: String, name: String) {
        store(id)
        self.name = name
        isNew = false
    }

    #if os(macOS)
    private static func stored() -> String? { UserDefaults.standard.string(forKey: service) }
    private static func store(_ id: String) { UserDefaults.standard.set(id, forKey: service) }
    #else
    private static var query: [String: Any] {
        [kSecClass as String: kSecClassGenericPassword,
         kSecAttrService as String: service,
         kSecAttrAccount as String: "id",
         kSecAttrSynchronizable as String: false] // this device's own, not shared through iCloud Keychain
    }

    private static func stored() -> String? {
        var q = query
        q[kSecReturnData as String] = true
        var out: CFTypeRef?
        guard SecItemCopyMatching(q as CFDictionary, &out) == errSecSuccess, let data = out as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    private static func store(_ id: String) {
        SecItemDelete(query as CFDictionary)
        var q = query
        q[kSecValueData as String] = Data(id.utf8)
        q[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        SecItemAdd(q as CFDictionary, nil)
    }
    #endif
}

/// The cabinets' saves in iCloud (CloudKit, the app's private database, in
/// a zone of its own): a Cabinet record per cabinet (name, model, last
/// seen) and a Save record per cabinet and game (its save files, when).
/// Everything here fails quietly without iCloud: the saves stay on the
/// device as before.
enum SaveSync {
    /// iCloud.<bundle ID>, as in the entitlements (APP_BUNDLE_ID in Signing.xcconfig).
    static let container = CKContainer(identifier: "iCloud." + (Bundle.main.bundleIdentifier ?? "com.thebrinkoftomorrow.arcadefoundry.insertcoin"))
    private static var db: CKDatabase { container.privateCloudDatabase }
    private static let zone = CKRecordZone.ID(zoneName: "Cabinets", ownerName: CKCurrentUserDefaultName)
    private static var zoneReady = false

    struct CabinetInfo: Identifiable, Hashable {
        let id: String
        let name: String
        let model: String
        let lastSeen: Date?
        var games: [String: Date] // romset: saved
    }

    static func available() async -> Bool {
        (try? await container.accountStatus()) == .available
    }

    private static func ensureZone() async throws {
        if zoneReady { return }
        _ = try await db.modifyRecordZones(saving: [CKRecordZone(zoneID: zone)], deleting: [])
        zoneReady = true
    }

    private static func cabinetID(_ id: String) -> CKRecord.ID { CKRecord.ID(recordName: "cabinet.\(id)", zoneID: zone) }
    private static func saveID(_ cabinet: String, _ romset: String) -> CKRecord.ID {
        CKRecord.ID(recordName: "save.\(cabinet).\(romset)", zoneID: zone)
    }

    private static func put(_ record: CKRecord) async throws {
        let (results, _) = try await db.modifyRecords(saving: [record], deleting: [], savePolicy: .allKeys, atomically: false)
        if case .failure(let e)? = results[record.recordID] { throw e }
    }

    /// Records this cabinet (its name, that it was seen now).
    static func announce() async {
        guard await available() else { return }
        do {
            try await ensureZone()
            let r = CKRecord(recordType: "Cabinet", recordID: cabinetID(Cabinet.id))
            r["name"] = Cabinet.name
            r["model"] = Cabinet.model
            r["lastSeen"] = Date()
            try await put(r)
        } catch {
            print("SaveSync: announce: \(error)")
        }
    }

    /// Backs up this cabinet's save files for `romset` (after the core has
    /// written them), dated when the game was played (the newest file). It
    /// stays pending until it reaches iCloud (see `retryPending`), and never
    /// replaces a backup played later on another device sharing this
    /// cabinet, unless `force`.
    static func upload(_ romset: String, force: Bool = false) async {
        let files = readFiles(Storage.saves(romset))
        guard !files.isEmpty else { return }
        setPending(romset, true)
        guard await available() else { return }
        do {
            try await ensureZone()
            let id = saveID(Cabinet.id, romset)
            let played = playedAt(romset) ?? Date()
            if !force, let saved = try await savedAt(id), saved >= played {
                setPending(romset, false)
                return
            }
            let r = CKRecord(recordType: "Save", recordID: id)
            r["cabinet"] = Cabinet.id
            r["romset"] = romset
            r["files"] = try PropertyListEncoder().encode(files)
            r["saved"] = played
            try await put(r)
            setPending(romset, false)
        } catch {
            print("SaveSync: upload \(romset): \(error)")
        }
    }

    /// Before a game starts: a pending backup of this device's save goes up
    /// first, then this cabinet's backup comes down when it was played later
    /// than this device's save (on another device sharing the cabinet, or
    /// this one lost its saves). Gives up after a few seconds offline: the
    /// game then starts from this device's save.
    static func sync(_ romset: String) async {
        await withTimeout(seconds: 6) {
            if isPending(romset) { await upload(romset) }
            guard let saved = try? await savedAt(saveID(Cabinet.id, romset)) else { return }
            if let played = playedAt(romset), played >= saved { return }
            _ = try? await download(cabinet: Cabinet.id, romset: romset)
        }
    }

    // MARK: Pending backups

    private static let pendingKey = "SaveSync.pending"
    private static let pendingLock = NSLock()
    private static let network = NWPathMonitor()

    /// Retries the pending backups whenever the network comes back (and once
    /// as the app starts). Nothing runs while none are pending.
    static func start() {
        network.pathUpdateHandler = { path in
            if path.status == .satisfied { Task { await retryPending() } }
        }
        network.start(queue: DispatchQueue(label: "SaveSync.network"))
    }

    /// Uploads the backups that didn't reach iCloud when their game ended.
    static func retryPending() async {
        for romset in pendingList() { await upload(romset) }
    }

    private static func pendingList() -> [String] {
        pendingLock.lock()
        defer { pendingLock.unlock() }
        return UserDefaults.standard.stringArray(forKey: pendingKey) ?? []
    }

    private static func isPending(_ romset: String) -> Bool {
        pendingLock.lock()
        defer { pendingLock.unlock() }
        return UserDefaults.standard.stringArray(forKey: pendingKey)?.contains(romset) ?? false
    }

    private static func setPending(_ romset: String, _ pending: Bool) {
        pendingLock.lock()
        defer { pendingLock.unlock() }
        var set = Set(UserDefaults.standard.stringArray(forKey: pendingKey) ?? [])
        if pending { set.insert(romset) } else { set.remove(romset) }
        UserDefaults.standard.set(set.sorted(), forKey: pendingKey)
    }

    /// When this device last played `romset`: its newest save file's date.
    private static func playedAt(_ romset: String) -> Date? {
        let dir = Storage.saves(romset)
        let names = (try? FileManager.default.contentsOfDirectory(atPath: dir.path)) ?? []
        return names.filter { !$0.hasPrefix(".") }.compactMap {
            try? FileManager.default.attributesOfItem(atPath: dir.appendingPathComponent($0).path)[.modificationDate] as? Date
        }.max()
    }

    /// When the backup `id` was played, or nil when there is none.
    private static func savedAt(_ id: CKRecord.ID) async throws -> Date? {
        let results = try await db.records(for: [id], desiredKeys: ["saved"])
        switch results[id] {
        case .success(let r)?:
            return r["saved"] as? Date
        case .failure(let e as CKError)? where e.code == .unknownItem || e.code == .zoneNotFound:
            return nil
        case .failure(let e)?:
            throw e
        case nil:
            return nil
        }
    }

    /// Writes cabinet `cabinet`'s saves for `romset` over this device's.
    /// Returns false when it has none.
    @discardableResult
    static func download(cabinet: String, romset: String) async throws -> Bool {
        let r: CKRecord
        do {
            r = try await db.record(for: saveID(cabinet, romset))
        } catch let e as CKError where e.code == .unknownItem || e.code == .zoneNotFound {
            return false
        }
        guard let data = r["files"] as? Data else { return false }
        let files = try PropertyListDecoder().decode([String: Data].self, from: data)
        let dir = Storage.saves(romset)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        // Dated when the backup was played, so this device's save counts as
        // that session, not a newer one.
        let saved = r["saved"] as? Date
        for (name, bytes) in files where !name.contains("/") {
            let url = dir.appendingPathComponent(name)
            try bytes.write(to: url, options: .atomic)
            if let saved { try? FileManager.default.setAttributes([.modificationDate: saved], ofItemAtPath: url.path) }
        }
        setPending(romset, false) // this device's own session, if not backed up, is replaced
        return true
    }

    /// Every cabinet in iCloud, with the games it has saves for.
    static func cabinets() async throws -> [CabinetInfo] {
        try await ensureZone()
        var found: [String: CabinetInfo] = [:]
        var saves: [(String, String, Date)] = []
        var token: CKServerChangeToken?
        var more = true
        while more {
            let changes = try await db.recordZoneChanges(inZoneWith: zone, since: token,
                                                         desiredKeys: ["name", "model", "lastSeen", "cabinet", "romset", "saved"])
            for case .success(let m) in changes.modificationResultsByID.values {
                let r = m.record
                if r.recordType == "Cabinet" {
                    let id = String(r.recordID.recordName.dropFirst("cabinet.".count))
                    found[id] = CabinetInfo(id: id, name: r["name"] as? String ?? "Cabinet", model: r["model"] as? String ?? "",
                                            lastSeen: r["lastSeen"] as? Date, games: [:])
                } else if r.recordType == "Save", let c = r["cabinet"] as? String, let s = r["romset"] as? String {
                    saves.append((c, s, r["saved"] as? Date ?? .distantPast))
                }
            }
            token = changes.changeToken
            more = changes.moreComing
        }
        for (c, s, d) in saves { found[c]?.games[s] = d }
        return found.values.sorted { ($0.lastSeen ?? .distantPast) > ($1.lastSeen ?? .distantPast) }
    }

    /// Takes over another cabinet (this device replaces it): its ID, name
    /// and every save it has, over this device's.
    static func adopt(_ other: CabinetInfo) async throws {
        Cabinet.adopt(id: other.id, name: other.name)
        await MainActor.run { LinkDiscovery.shared.advertise() }
        for romset in other.games.keys {
            try await download(cabinet: other.id, romset: romset)
        }
        await announce()
    }

    /// Makes this device a cabinet of its own (Cabinet.renew), records it,
    /// and backs up every game save it has under the new ID. The cabinet it
    /// left keeps its backups for the other device.
    static func becomeNew() async {
        Cabinet.renew()
        await MainActor.run { LinkDiscovery.shared.advertise() }
        await announce()
        for game in GameInfo.all where !readFiles(Storage.saves(game.id)).isEmpty {
            await upload(game.id)
        }
    }

    /// Copies one game's save from another cabinet to this one (its link
    /// role comes with it), and backs it up as this cabinet's.
    static func copy(_ romset: String, from other: CabinetInfo) async throws {
        try await download(cabinet: other.id, romset: romset)
        await upload(romset, force: true)
    }

    /// Deletes a cabinet and its saves from iCloud.
    static func delete(_ other: CabinetInfo) async throws {
        let ids = [cabinetID(other.id)] + other.games.keys.map { saveID(other.id, $0) }
        _ = try await db.modifyRecords(saving: [], deleting: ids)
    }

    private static func readFiles(_ dir: URL) -> [String: Data] {
        let fm = FileManager.default
        var out: [String: Data] = [:]
        for name in (try? fm.contentsOfDirectory(atPath: dir.path)) ?? [] where !name.hasPrefix(".") {
            if let d = fm.contents(atPath: dir.appendingPathComponent(name).path) { out[name] = d }
        }
        return out
    }

    private static func withTimeout(seconds: Double, _ work: @escaping @Sendable () async -> Void) async {
        await withTaskGroup(of: Void.self) { group in
            group.addTask { await work() }
            group.addTask { try? await Task.sleep(nanoseconds: UInt64(seconds * 1e9)) }
            await group.next()
            group.cancelAll()
        }
    }
}
