import CoreVideo
import Foundation

/// One game's core library (afcore_<romset>.framework, built by
/// scripts/build_cores.py): found in the app's Frameworks folder and opened
/// at run time, so the app runs with whichever cores it was built with.
final class CoreLibrary {
    let romset: String
    let api: UnsafePointer<afcore_api>
    private let handle: UnsafeMutableRawPointer

    private init(romset: String, handle: UnsafeMutableRawPointer, api: UnsafePointer<afcore_api>) {
        self.romset = romset
        self.handle = handle
        self.api = api
    }

    var title: String { String(cString: api.pointee.title) }
    var frameHz: Double { api.pointee.frame_hz }
    /// This game's core version, x.y.z.
    var version: String { Self.format(api.pointee.core_version) }

    /// The core interface this app is built for (include/afcore.h). A core
    /// loads when its major version matches.
    static let interfaceVersion = "\(AFCORE_API_MAJOR).\(AFCORE_API_MINOR).\(AFCORE_API_PATCH)"

    /// x.y.z; a core from before 4.0.0 numbered its interface 1, 2, 3.
    static func format(_ v: UInt32) -> String { v < 1 << 16 ? "\(v)" : "\(v >> 16).\((v >> 8) & 0xff).\(v & 0xff)" }

    private static var cache: [String: CoreLibrary] = [:]
    /// Cores in this build made for another interface: romset to its version.
    private static var outdated: [String: UInt32] = [:]

    /// Why `romset` has no core (load returned nil).
    static func unavailable(_ romset: String) -> String {
        if let v = outdated[romset] {
            return "Its core is for interface \(format(v)), this app needs \(AFCORE_API_MAJOR).x: rebuild it with scripts/build_cores.py, then build the app"
        }
        return "Not in this build: run scripts/build_cores.py with its ROM set"
    }

    /// Whether this build has a core for `romset` made for this interface,
    /// without opening it (#193): the library opened every core (dlopen) to
    /// draw its cards, seconds at launch. build_cores.py writes the interface
    /// version a core was built for into its framework (afcore-interface,
    /// #195); one built before that is only checked when it is opened.
    static func exists(_ romset: String) -> Bool {
        if cache[romset] != nil { return true }
        if outdated[romset] != nil { return false }
        if let known = present[romset] { return known }
        var found = binary(romset).map { FileManager.default.fileExists(atPath: $0.path) } ?? false
        if found, let v = builtFor(romset), v >> 16 != UInt32(AFCORE_API_MAJOR) {
            outdated[romset] = v
            found = false
        }
        present[romset] = found
        return found
    }

    /// The interface version x.y.z (packed) in a core's afcore-interface:
    /// Resources/ in a Mac framework, the top of an iOS or tvOS one.
    private static func builtFor(_ romset: String) -> UInt32? {
        guard let fw = binary(romset)?.deletingLastPathComponent() else { return nil }
        for file in [fw.appendingPathComponent("Resources/afcore-interface"), fw.appendingPathComponent("afcore-interface")] {
            guard let text = try? String(contentsOf: file, encoding: .utf8) else { continue }
            let parts = text.trimmingCharacters(in: .whitespacesAndNewlines).split(separator: ".").compactMap { UInt32($0) }
            guard parts.count == 3 else { return nil }
            return parts[0] << 16 | parts[1] << 8 | parts[2]
        }
        return nil
    }

    private static var present: [String: Bool] = [:]

    /// This build's core for `romset` is for another interface (after
    /// `exists` or `load` looked).
    static func isOutdated(_ romset: String) -> Bool { outdated[romset] != nil }

    private static func binary(_ romset: String) -> URL? {
        let name = "afcore_\(romset)"
        return Bundle.main.privateFrameworksURL?.appendingPathComponent("\(name).framework").appendingPathComponent(name)
    }

    /// The core for `romset`, or nil when this build does not have it (or
    /// has one for another major version of the interface).
    static func load(_ romset: String) -> CoreLibrary? {
        if let lib = cache[romset] { return lib }
        if outdated[romset] != nil || present[romset] == false { return nil }
        guard let binary = binary(romset) else { return nil }
        guard let handle = dlopen(binary.path, RTLD_NOW | RTLD_LOCAL) else { return nil }
        guard let sym = dlsym(handle, "afcore_api_\(romset)") else {
            dlclose(handle)
            return nil
        }
        typealias Entry = @convention(c) () -> UnsafePointer<afcore_api>?
        guard let api = unsafeBitCast(sym, to: Entry.self)() else {
            dlclose(handle)
            return nil
        }
        // A core from before 4.0.0 has no size or core_version: only its first field is read.
        guard api.pointee.version >> 16 == UInt32(AFCORE_API_MAJOR) else {
            outdated[romset] = api.pointee.version
            dlclose(handle)
            return nil
        }
        let lib = CoreLibrary(romset: romset, handle: handle, api: api)
        cache[romset] = lib
        return lib
    }
}

enum CoreError: LocalizedError {
    case failed(String)
    var errorDescription: String? {
        switch self {
        case .failed(let why): return why
        }
    }
}

/// A running game: one afcore instance. Used from one thread at a time.
final class GameCore {
    let library: CoreLibrary
    let core: OpaquePointer
    private var api: afcore_api { library.api.pointee }

    init(library: CoreLibrary, romURL: URL, saveDir: URL) throws {
        self.library = library
        var err = [CChar](repeating: 0, count: 512)
        try FileManager.default.createDirectory(at: saveDir, withIntermediateDirectories: true)
        guard let c = library.api.pointee.create!(romURL.path, saveDir.path, &err, err.count) else {
            throw CoreError.failed(String(cString: err))
        }
        core = c
    }

    deinit { api.destroy!(core) }

    func runFrame(_ input: afcore_input) {
        var input = input
        api.run_frame!(core, &input)
    }

    /// Why the core stopped (it cannot run on from here), or nil while it runs.
    func error() -> String? {
        guard let reason = api.error?(core) else { return nil }
        return String(cString: reason)
    }

    /// The screen as BGRA pixels, valid until the next runFrame.
    func screen() -> (pixels: UnsafePointer<UInt32>, width: Int, height: Int) {
        var w: Int32 = 0, h: Int32 = 0
        let p = api.screen!(core, &w, &h)!
        return (p, Int(w), Int(h))
    }

    /// The screen as an IOSurface-backed BGRA pixel buffer the GPU has
    /// finished drawing (afcore.h 5.1, a Lindbergh core on iOS), for the view
    /// to show without a copy; nil from a core without it: use screen().
    func screenSurface() -> CVPixelBuffer? {
        let api = library.api.pointee
        guard let at = MemoryLayout<afcore_api>.offset(of: \afcore_api.screen_surface),
              Int(api.size) >= at + MemoryLayout<UnsafeRawPointer>.size, let get = api.screen_surface else { return nil }
        var w: Int32 = 0, h: Int32 = 0
        guard let p = get(core, &w, &h) else { return nil }
        return Unmanaged<CVPixelBuffer>.fromOpaque(p).takeUnretainedValue()
    }

    func audioRate(_ stream: Int) -> Double { api.audio_rate!(core, Int32(stream)) }

    /// Interleaved stereo frames from `stream` into `buffer`; how many.
    func readAudio(_ stream: Int, into buffer: UnsafeMutablePointer<Float>, maxFrames: Int) -> Int {
        Int(api.audio_read!(core, Int32(stream), buffer, maxFrames))
    }

    func save() { api.save!(core) }

    func startLink(listenPort: Int, next: String, frameSync: Bool) throws {
        var err = [CChar](repeating: 0, count: 512)
        if api.link_start!(core, Int32(listenPort), next, frameSync ? 1 : 0, &err, err.count) != 0 {
            throw CoreError.failed(String(cString: err))
        }
    }

    enum LinkState { case off, waiting, up(id: Int, count: Int), lost }

    var linkState: LinkState {
        var id: Int32 = 0, count: Int32 = 0
        switch api.link_state!(core, &id, &count) {
        case Int32(AFCORE_LINK_WAITING): return .waiting
        case Int32(AFCORE_LINK_UP): return .up(id: Int(id), count: Int(count))
        case Int32(AFCORE_LINK_LOST): return .lost
        default: return .off
        }
    }
}
