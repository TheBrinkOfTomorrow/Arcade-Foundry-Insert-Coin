import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

/// Each game's picture in the library: a screen from the game (taken by
/// itself after half a minute of the first play, or chosen in the pause
/// menu) or an image the user picked in the game's settings.
/// Pictures/<romset>.png beside the saves, 4:3; not backed up, since playing
/// makes a new one.
enum Pictures {
    static func url(_ romset: String) -> URL {
        Storage.root.appendingPathComponent("Pictures/\(romset).png")
    }

    static func exists(_ romset: String) -> Bool { FileManager.default.fileExists(atPath: url(romset).path) }

    /// Loaded pictures and the file dates they were loaded at.
    private static var cache: [String: (date: Date, image: CGImage)] = [:]

    static func image(_ romset: String) -> CGImage? {
        let file = url(romset)
        guard let date = (try? FileManager.default.attributesOfItem(atPath: file.path))?[.modificationDate] as? Date else {
            cache[romset] = nil
            return nil
        }
        if let hit = cache[romset], hit.date == date { return hit.image }
        guard let source = CGImageSourceCreateWithURL(file as CFURL, nil),
              let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else { return nil }
        cache[romset] = (date, image)
        return image
    }

    /// The game's screen (BGRA, GameCore.screen) as its picture, redrawn
    /// at 4:3 with square pixels (the board's are not).
    static func save(_ romset: String, pixels: UnsafePointer<UInt32>, width: Int, height: Int) {
        let data = Data(bytes: pixels, count: width * height * 4)
        let info = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)
        guard let provider = CGDataProvider(data: data as CFData),
              let screen = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
                                   bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: info,
                                   provider: provider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)
        else { return }
        write(screen, romset, size: (height * 4 / 3, height))
    }

    /// An image file the user picked (any size: shown filling a 4:3 card).
    static func use(_ file: URL, for romset: String) throws {
        let scoped = file.startAccessingSecurityScopedResource()
        defer { if scoped { file.stopAccessingSecurityScopedResource() } }
        guard let source = CGImageSourceCreateWithURL(file as CFURL, nil),
              let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
            throw CocoaError(.fileReadCorruptFile)
        }
        // At most 1024 across, so a photo doesn't fill the cache.
        let scale = min(1, 1024 / Double(max(image.width, image.height)))
        write(image, romset, size: (Int(Double(image.width) * scale), Int(Double(image.height) * scale)))
    }

    /// The game takes a new one the next time it is played.
    static func remove(_ romset: String) {
        try? FileManager.default.removeItem(at: url(romset))
        cache[romset] = nil
    }

    private static func write(_ image: CGImage, _ romset: String, size: (Int, Int)) {
        let (w, h) = size
        let info = CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue
        guard w > 0, h > 0,
              let ctx = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: 0,
                                  space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: info)
        else { return }
        ctx.interpolationQuality = .high
        ctx.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
        guard let scaled = ctx.makeImage() else { return }
        let file = url(romset)
        try? FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
        guard let dest = CGImageDestinationCreateWithURL(file as CFURL, UTType.png.identifier as CFString, 1, nil) else { return }
        CGImageDestinationAddImage(dest, scaled, nil)
        CGImageDestinationFinalize(dest)
        cache[romset] = nil
    }
}
