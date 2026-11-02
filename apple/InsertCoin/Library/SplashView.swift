import SwiftUI

/// The title screen: the logo on navy, over the library from the first frame
/// (iPhone, iPad and Apple TV show the same picture as their launch screen)
/// and faded out once the app is up. The logo is as wide as on the launch
/// screen (LaunchLogo, made by scripts/make_icons.py), so nothing jumps,
/// and never wider than the screen.
struct SplashView: View {
    var body: some View {
        GeometryReader { geo in
            ZStack {
                Color("LaunchBackground")
                Image("Logo")
                    .resizable()
                    .scaledToFit()
                    .frame(width: max(0, min(Self.logoWidth, geo.size.width - 48)))
                    .frame(maxHeight: max(0, geo.size.height - 48))
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
        .ignoresSafeArea()
    }

    /// The launch screen's logo width in points (make_icons.py's LAUNCH_LOGO).
    private static var logoWidth: CGFloat {
        #if os(tvOS)
        return 800
        #elseif os(iOS)
        return UIDevice.current.userInterfaceIdiom == .pad ? 520 : 280
        #else
        return 640
        #endif
    }
}
