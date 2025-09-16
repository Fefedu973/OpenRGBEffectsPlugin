#-----------------------------------------------------------------------------------------------#
# OpenRGB Effects Plugin QMake Project                                                          #
#-----------------------------------------------------------------------------------------------#

#-----------------------------------------------------------------------------------------------#
# Qt Configuration                                                                              #
#-----------------------------------------------------------------------------------------------#
QT +=                                                                                           \
    core                                                                                        \
    gui                                                                                         \
    opengl                                                                                      \
    widgets

if(greaterThan(QT_MAJOR_VERSION, 5)) {
QT +=                                                                                           \
    core5compat                                                                                 \
}

DEFINES += OPEN_RGB_EFFECTS_PLUGIN_LIBRARY
TEMPLATE = lib

#-----------------------------------------------------------------------------------------------#
# Build Configuration                                                                           #
#-----------------------------------------------------------------------------------------------#
CONFIG +=                                                                                       \
    embed_translations                                                                          \
    lrelease                                                                                    \
    plugin                                                                                      \
    silent                                                                                      \

#-----------------------------------------------------------------------------------------------#
# Application Configuration                                                                     #
#-----------------------------------------------------------------------------------------------#
MAJOR       = 0
MINOR       = 9
SUFFIX      = git

SHORTHASH   = $$system("git rev-parse --short=7 HEAD")
LASTTAG     = "release_"$$MAJOR"."$$MINOR
COMMAND     = "git rev-list --count "$$LASTTAG"..HEAD"
COMMITS     = $$system($$COMMAND)

VERSION_NUM = $$MAJOR"."$$MINOR"."$$COMMITS
VERSION_STR = $$MAJOR"."$$MINOR

VERSION_DEB = $$VERSION_NUM
VERSION_WIX = $$VERSION_NUM
VERSION_AUR = $$VERSION_NUM
VERSION_RPM = $$VERSION_NUM

equals(SUFFIX, "git") {
VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX$$COMMITS")"
VERSION_DEB = $$VERSION_DEB"~git"$$SHORTHASH
VERSION_AUR = $$VERSION_AUR".g"$$SHORTHASH
VERSION_RPM = $$VERSION_RPM"^git"$$SHORTHASH
} else {
    !isEmpty(SUFFIX) {
VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX")"
VERSION_DEB = $$VERSION_DEB"~"$$SUFFIX
VERSION_AUR = $$VERSION_AUR"."$$SUFFIX
VERSION_RPM = $$VERSION_RPM"^"$$SUFFIX
    }
}

message("VERSION_NUM: "$$VERSION_NUM)
message("VERSION_STR: "$$VERSION_STR)
message("VERSION_DEB: "$$VERSION_DEB)
message("VERSION_WIX: "$$VERSION_WIX)
message("VERSION_AUR: "$$VERSION_AUR)
message("VERSION_RPM: "$$VERSION_RPM)

#-----------------------------------------------------------------------------------------------#
# Automatically generated build information                                                     #
#-----------------------------------------------------------------------------------------------#
win32:BUILDDATE = $$system(date /t)
unix:BUILDDATE  = $$system(date -R -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}")
GIT_COMMIT_ID   = $$system(git log -n 1 --pretty=format:"%H")
GIT_COMMIT_DATE = $$system(git log -n 1 --pretty=format:"%ci")
GIT_BRANCH      = $$system(git branch --show-current)

#-----------------------------------------------------------------------------------------------#
# Download links                                                                                #
#-----------------------------------------------------------------------------------------------#
win32:LATEST_BUILD_URL="https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin/-/jobs/artifacts/master/download?job=Windows 64"
unix:!macx:LATEST_BUILD_URL="https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin/-/jobs/artifacts/master/download?job=Linux 64"

#-----------------------------------------------------------------------------------------------#
# Inject vars in defines                                                                        #
#-----------------------------------------------------------------------------------------------#
DEFINES +=                                                                                      \
    VERSION_STRING=\\"\"\"$$VERSION_STR\\"\"\"                                                  \
    BUILDDATE_STRING=\\"\"\"$$BUILDDATE\\"\"\"                                                  \
    GIT_COMMIT_ID=\\"\"\"$$GIT_COMMIT_ID\\"\"\"                                                 \
    GIT_COMMIT_DATE=\\"\"\"$$GIT_COMMIT_DATE\\"\"\"                                             \
    GIT_BRANCH=\\"\"\"$$GIT_BRANCH\\"\"\"                                                       \
    LATEST_BUILD_URL=\\"\"\"$$LATEST_BUILD_URL\\"\"\"                                           \
    SHADERS_README=\\"\"\"https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin/-/blob/master/Effects/Shaders/README.md\\"\"\"                                               \

#-----------------------------------------------------------------------------------------------#
# OpenRGB Plugin SDK                                                                            #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    OpenRGB                                                                                     \
    OpenRGB/RGBController                                                                       \
    OpenRGB/dependencies/json                                                                   \
    OpenRGB/qt                                                                                  \
    OpenRGB/i2c_smbus                                                                           \
    OpenRGB/net_port                                                                            \

HEADERS +=                                                                                      \
    OpenRGB/Colors.h                                                                            \
    OpenRGB/OpenRGBPluginInterface.h                                                            \
    OpenRGB/ResourceManagerInterface.h                                                          \


SOURCES +=                                                                                      \
    OpenRGB/RGBController/RGBController.cpp                                                     \
    OpenRGB/RGBController/RGBController_Network.cpp                                             \
    OpenRGB/NetworkServer.cpp                                                                   \
    OpenRGB/NetworkClient.cpp                                                                   \
    OpenRGB/NetworkProtocol.cpp                                                                 \
    OpenRGB/LogManager.cpp                                                                      \
    OpenRGB/net_port/net_port.cpp                                                               \
    OpenRGB/qt/hsv.cpp                                                                          \



#-----------------------------------------------------------------------------------------------#
# QCodeEditor                                                                                   #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    Dependencies/QCodeEditor/include                                                            \
    Dependencies/QCodeEditor/include/internal                                                   \

HEADERS +=                                                                                      \
    Dependencies/QCodeEditor/include/internal/QCodeEditor.hpp                                   \
    Dependencies/QCodeEditor/include/internal/QGLSLCompleter.hpp                                \
    Dependencies/QCodeEditor/include/internal/QHighlightRule.hpp                                \
    Dependencies/QCodeEditor/include/internal/QLineNumberArea.hpp                               \
    Dependencies/QCodeEditor/include/internal/QPythonCompleter.hpp                              \
    Dependencies/QCodeEditor/include/internal/QSyntaxStyle.hpp                                  \
    Dependencies/QCodeEditor/include/internal/QCXXHighlighter.hpp                               \
    Dependencies/QCodeEditor/include/internal/QGLSLHighlighter.hpp                              \
    Dependencies/QCodeEditor/include/internal/QJSONHighlighter.hpp                              \
    Dependencies/QCodeEditor/include/internal/QLuaCompleter.hpp                                 \
    Dependencies/QCodeEditor/include/internal/QPythonHighlighter.hpp                            \
    Dependencies/QCodeEditor/include/internal/QXMLHighlighter.hpp                               \
    Dependencies/QCodeEditor/include/internal/QHighlightBlockRule.hpp                           \
    Dependencies/QCodeEditor/include/internal/QLanguage.hpp                                     \
    Dependencies/QCodeEditor/include/internal/QLuaHighlighter.hpp                               \
    Dependencies/QCodeEditor/include/internal/QStyleSyntaxHighlighter.hpp                       \
    Dependencies/QCodeEditor/include/internal/QJSHighlighter.hpp                                \
    Dependencies/QCodeEditor/include/internal/QJavaHighlighter.hpp                              \

SOURCES +=                                                                                      \
    Dependencies/QCodeEditor/src/internal/QCodeEditor.cpp                                       \
    Dependencies/QCodeEditor/src/internal/QGLSLCompleter.cpp                                    \
    Dependencies/QCodeEditor/src/internal/QLanguage.cpp                                         \
    Dependencies/QCodeEditor/src/internal/QLuaHighlighter.cpp                                   \
    Dependencies/QCodeEditor/src/internal/QStyleSyntaxHighlighter.cpp                           \
    Dependencies/QCodeEditor/src/internal/QCXXHighlighter.cpp                                   \
    Dependencies/QCodeEditor/src/internal/QGLSLHighlighter.cpp                                  \
    Dependencies/QCodeEditor/src/internal/QLineNumberArea.cpp                                   \
    Dependencies/QCodeEditor/src/internal/QPythonCompleter.cpp                                  \
    Dependencies/QCodeEditor/src/internal/QSyntaxStyle.cpp                                      \
    Dependencies/QCodeEditor/src/internal/QJSONHighlighter.cpp                                  \
    Dependencies/QCodeEditor/src/internal/QLuaCompleter.cpp                                     \
    Dependencies/QCodeEditor/src/internal/QPythonHighlighter.cpp                                \
    Dependencies/QCodeEditor/src/internal/QXMLHighlighter.cpp                                   \
    Dependencies/QCodeEditor/src/internal/QJSHighlighter.cpp                                    \
    Dependencies/QCodeEditor/src/internal/QJavaHighlighter.cpp                                  \
    Dependencies/QCodeEditor/src/internal/QtCompat.hpp                                          \

RESOURCES +=                                                                                    \
    Dependencies/QCodeEditor/resources/qcodeeditor_resources.qrc

#-----------------------------------------------------------------------------------------------#
# ctkrangeslider                                                                                #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    Dependencies/ctkrangeslider                                                                 \

HEADERS +=                                                                                      \
    Dependencies/ctkrangeslider/ctkrangeslider.h                                                \

SOURCES +=                                                                                      \
    Dependencies/ctkrangeslider/ctkrangeslider.cpp                                              \

#-----------------------------------------------------------------------------------------------#
# chuck_fft                                                                                     #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    Dependencies/chuck_fft                                                                      \

SOURCES +=                                                                                      \
    Dependencies/chuck_fft/chuck_fft.c

HEADERS +=                                                                                      \
    Dependencies/chuck_fft/chuck_fft.h                                                          \

#-----------------------------------------------------------------------------------------------#
# SimplexNoise                                                                                  #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    Dependencies/SimplexNoise/src                                                               \

SOURCES +=                                                                                      \
    Dependencies/SimplexNoise/src/SimplexNoise.cpp                                              \

HEADERS +=                                                                                      \
    Dependencies/SimplexNoise/src/SimplexNoise.h                                                \

#-----------------------------------------------------------------------------------------------#
# GUI and misc                                                                                  #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH += \
    ScreenCapturer                                                                              \
    ScreenCapturer/windows                                                                      \
    ScreenCapturer/qt                                                                           \
    ScreenCapturer/linux/wayland                                                                \
    Audio                                                                                       \

HEADERS +=                                                                                      \
    Audio/AudioDataStruct.h                                                                     \
    Audio/AudioSettings.h                                                                       \
    Audio/AudioSettingsStruct.h                                                                 \
    Audio/AudioSignalProcessor.h                                                                \
    ColorPicker.h                                                                               \
    ColorsPicker.h                                                                              \
    ColorUtils.h                                                                                \
    ControllerZone.h                                                                            \
    DeviceList.h                                                                                \
    DeviceListItem.h                                                                            \
    EffectList.h                                                                                \
    EffectListManager.h                                                                         \
    EffectManager.h                                                                             \
    EffectsName.h                                                                               \
    EffectSearch.h                                                                              \
    EffectTabHeader.h                                                                           \
    Effects/EffectRegisterer.h                                                                  \
    Effects/RGBEffect.h                                                                         \
    LivePreviewController.h                                                                     \
    OpenRGBEffectPage.h                                                                         \
    OpenRGBEffectsPlugin.h                                                                      \
    OpenRGBEffectSettings.h                                                                     \
    OpenRGBEffectTab.h                                                                          \
    PluginInfo.h                                                                                \
    PreviewWidget.h                                                                             \
    QTooltipedSlider.h                                                                          \
    SaveProfilePopup.h                                                                          \
    ZoneListItem.h                                                                              \
    OpenRGBPluginsFont.h                                                                        \
    GlobalSettings.h                                                                            \
    ScreenCapturer/ScreenCapturer.h                                                             \
    ScreenCapturer/qt/QtScreenCapturer.h                                                        \
    ScreenCapturer/windows/WindowsScreenCapturer.h                                              \

SOURCES +=                                                                                      \
    Audio/AudioSettings.cpp                                                                     \
    Audio/AudioSettingsStruct.cpp                                                               \
    Audio/AudioSignalProcessor.cpp                                                              \
    Audio/AudioManager.cpp                                                                      \
    ColorPicker.cpp                                                                             \
    ColorsPicker.cpp                                                                            \
    DeviceList.cpp                                                                              \
    DeviceListItem.cpp                                                                          \
    EffectList.cpp                                                                              \
    EffectListManager.cpp                                                                       \
    EffectManager.cpp                                                                           \
    EffectSearch.cpp                                                                            \
    EffectTabHeader.cpp                                                                         \
    LivePreviewController.cpp                                                                   \
    OpenRGBEffectPage.cpp                                                                       \
    OpenRGBEffectsPlugin.cpp                                                                    \
    OpenRGBEffectSettings.cpp                                                                   \
    OpenRGBEffectTab.cpp                                                                        \
    PluginInfo.cpp                                                                              \
    QTooltipedSlider.cpp                                                                        \
    PreviewWidget.cpp                                                                           \
    SaveProfilePopup.cpp                                                                        \
    ZoneListItem.cpp                                                                            \
    OpenRGBPluginsFont.cpp                                                                      \
    GlobalSettings.cpp                                                                          \
    ScreenCapturer/qt/QtScreenCapturer.cpp                                                      \
    ScreenCapturer/windows/WindowsScreenCapturer.cpp


FORMS +=                                                                                        \
    Audio/AudioSettings.ui                                                                      \
    ColorPicker.ui                                                                              \
    ColorsPicker.ui                                                                             \
    DeviceList.ui                                                                               \
    DeviceListItem.ui                                                                           \
    EffectList.ui                                                                               \
    EffectSearch.ui                                                                             \
    EffectTabHeader.ui                                                                          \
    LivePreviewController.ui                                                                    \
    OpenRGBEffectPage.ui                                                                        \
    OpenRGBEffectTab.ui                                                                         \
    PluginInfo.ui                                                                               \
    SaveProfilePopup.ui                                                                         \
    ZoneListItem.ui                                                                             \
    GlobalSettings.ui

#-----------------------------------------------------------------------------------------------#
# Effects                                                                                       #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    Effects                                                                                     \
    Audio                                                                                       \

SOURCES +=                                                                                      \
    Effects/Ambient/Ambient.cpp                                                                 \
    Effects/Ambient/RectangleSelector.cpp                                                       \
    Effects/AudioBubbles/AudioBubbles.cpp                                                       \
    Effects/AudioParty/AudioParty.cpp                                                           \
    Effects/AudioSine/AudioSine.cpp                                                             \
    Effects/AudioStar/AudioStar.cpp                                                             \
    Effects/AudioSync/AudioSync.cpp                                                             \
    Effects/AudioVisualizer/AudioVisualizer.cpp                                                 \
    Effects/AudioVUMeter/AudioVUMeter.cpp                                                       \
    Effects/Bloom/Bloom.cpp                                                                     \
    Effects/BouncingBall/BouncingBall.cpp                                                       \
    Effects/BouncingBall/BouncingBallSimulation.cpp                                             \
    Effects/Breathing/Breathing.cpp                                                             \
    Effects/BreathingCircle/BreathingCircle.cpp                                                 \
    Effects/Bubbles/Bubbles.cpp                                                                 \
    Effects/Clock/Clock.cpp                                                                     \
    Effects/ColorWheel/ColorWheel.cpp                                                           \
    Effects/Comet/Comet.cpp                                                                     \
    Effects/CrossingBeams/CrossingBeams.cpp                                                     \
    Effects/CustomBlink/CustomBlink.cpp                                                         \
    Effects/CustomGradientWave/CustomGradientWave.cpp                                           \
    Effects/CustomMarquee/CustomMarquee.cpp                                                     \
    Effects/DoubleRotatingRainbow/DoubleRotatingRainbow.cpp                                     \
    Effects/Fill/Fill.cpp                                                                       \
    Effects/FractalMotion/FractalMotion.cpp                                                     \
    Effects/GifPlayer/GifPlayer.cpp                                                             \
    Effects/GradientWave/GradientWave.cpp                                                       \
    Effects/Hypnotoad/Hypnotoad.cpp                                                             \
    Effects/Layers/Layers.cpp                                                                   \
    Effects/Layers/LayerEntry.cpp                                                               \
    Effects/Layers/LayerGroupEntry.cpp                                                          \
    Effects/Lightning/Lightning.cpp                                                             \
    Effects/Marquee/Marquee.cpp                                                                 \
    Effects/Mask/Mask.cpp                                                                       \
    Effects/Mosaic/Mosaic.cpp                                                                   \
    Effects/MotionPoint/MotionPoint.cpp                                                         \
    Effects/MotionPoints/MotionPoints.cpp                                                       \
    Effects/MovingPanes/MovingPanes.cpp                                                         \
    Effects/NoiseMap/NoiseMap.cpp                                                               \
    Effects/Policing/Policing.cpp                                                               \
    Effects/RadialRainbow/RadialRainbow.cpp                                                     \
    Effects/Rain/Rain.cpp                                                                       \
    Effects/RainbowWave/RainbowWave.cpp                                                         \
    Effects/RandomMarquee/RandomMarquee.cpp                                                     \
    Effects/RandomSpin/RandomSpin.cpp                                                           \
    Effects/RotatingBeam/RotatingBeam.cpp                                                       \
    Effects/RotatingRainbow/RotatingRainbow.cpp                                                 \
    Effects/Sequence/Sequence.cpp                                                               \
    Effects/Shaders/NewShaderPassTabHeader.cpp                                                  \
    Effects/Shaders/ShaderFileTabHeader.cpp                                                     \
    Effects/Shaders/ShaderPass.cpp                                                              \
    Effects/Shaders/ShaderPassEditor.cpp                                                        \
    Effects/Shaders/ShaderProgram.cpp                                                           \
    Effects/Shaders/Shaders.cpp                                                                 \
    Effects/Shaders/ShaderRenderer.cpp                                                          \
    Effects/Shaders/GLSLHighlighter.cpp                                                         \
    Effects/Shaders/GLSLCodeEditor.cpp                                                          \
    Effects/SmoothBlink/SmoothBlink.cpp                                                         \
    Effects/SpectrumCycling/SpectrumCycling.cpp                                                 \
    Effects/SparkleFade/SparkleFade.cpp                                                         \
    Effects/Spiral/Spiral.cpp                                                                   \
    Effects/Stack/Stack.cpp                                                                     \
    Effects/StarryNight/StarryNight.cpp                                                         \
    Effects/Sunrise/Sunrise.cpp                                                                 \
    Effects/Swap/Swap.cpp                                                                       \
    Effects/SwirlCircles/SwirlCircles.cpp                                                       \
    Effects/SwirlCirclesAudio/SwirlCirclesAudio.cpp                                             \
    Effects/Visor/Visor.cpp                                                                     \
    Effects/Wavy/Wavy.cpp                                                                       \
    Effects/ZigZag/ZigZag.cpp                                                                   \

HEADERS +=                                                                                      \
    Effects/Ambient/Ambient.h                                                                   \
    Effects/Ambient/RectangleSelector.h                                                         \
    Effects/AudioBubbles/AudioBubbles.h                                                         \
    Effects/AudioParty/AudioParty.h                                                             \
    Effects/AudioSine/AudioSine.h                                                               \
    Effects/AudioStar/AudioStar.h                                                               \
    Effects/AudioSync/AudioSync.h                                                               \
    Effects/AudioVisualizer/AudioVisualizer.h                                                   \
    Effects/AudioVUMeter/AudioVUMeter.h                                                         \
    Effects/Bloom/Bloom.h                                                                       \
    Effects/BouncingBall/BouncingBall.h                                                         \
    Effects/BouncingBall/BouncingBallSimulation.h                                               \
    Effects/Breathing/Breathing.h                                                               \
    Effects/BreathingCircle/BreathingCircle.h                                                   \
    Effects/Bubbles/Bubbles.h                                                                   \
    Effects/Clock/Clock.h                                                                       \
    Effects/ColorWheel/ColorWheel.h                                                             \
    Effects/Comet/Comet.h                                                                       \
    Effects/CrossingBeams/CrossingBeams.h                                                       \
    Effects/CustomBlink/CustomBlink.h                                                           \
    Effects/CustomGradientWave/CustomGradientWave.h                                             \
    Effects/CustomMarquee/CustomMarquee.h                                                       \
    Effects/DoubleRotatingRainbow/DoubleRotatingRainbow.h                                       \
    Effects/Fill/Fill.h                                                                         \
    Effects/FractalMotion/FractalMotion.h                                                       \
    Effects/GifPlayer/GifPlayer.h                                                               \
    Effects/GradientWave/GradientWave.h                                                         \
    Effects/Hypnotoad/Hypnotoad.h                                                               \
    Effects/Layers/Layers.h                                                                     \
    Effects/Layers/LayerEntry.h                                                                 \
    Effects/Layers/LayerGroupEntry.h                                                            \
    Effects/Lightning/Lightning.h                                                               \
    Effects/Marquee/Marquee.h                                                                   \
    Effects/Mask/Mask.h                                                                         \
    Effects/Mosaic/Mosaic.h                                                                     \
    Effects/MotionPoint/MotionPoint.h                                                           \
    Effects/MotionPoints/MotionPoints.h                                                         \
    Effects/MovingPanes/MovingPanes.h                                                           \
    Effects/NoiseMap/NoiseMap.h                                                                 \
    Effects/Policing/Policing.h                                                                 \
    Effects/RadialRainbow/RadialRainbow.h                                                       \
    Effects/Rain/Rain.h                                                                         \
    Effects/RainbowWave/RainbowWave.h                                                           \
    Effects/RandomMarquee/RandomMarquee.h                                                       \
    Effects/RandomSpin/RandomSpin.h                                                             \
    Effects/RotatingBeam/RotatingBeam.h                                                         \
    Effects/RotatingRainbow/RotatingRainbow.h                                                   \
    Effects/Sequence/Sequence.h                                                                 \
    Effects/Shaders/NewShaderPassTabHeader.h                                                    \
    Effects/Shaders/ShaderFileTabHeader.h                                                       \
    Effects/Shaders/ShaderPass.h                                                                \
    Effects/Shaders/ShaderPassData.h                                                            \
    Effects/Shaders/ShaderPassEditor.h                                                          \
    Effects/Shaders/ShaderProgram.h                                                             \
    Effects/Shaders/Shaders.h                                                                   \
    Effects/Shaders/ShaderRenderer.h                                                            \
    Effects/Shaders/GLSLHighlighter.h                                                           \
    Effects/Shaders/GLSLCodeEditor.h                                                            \
    Effects/SmoothBlink/SmoothBlink.h                                                           \
    Effects/SparkleFade/SparkleFade.h                                                           \
    Effects/SpectrumCycling/SpectrumCycling.h                                                   \
    Effects/Spiral/Spiral.h                                                                     \
    Effects/Stack/Stack.h                                                                       \
    Effects/StarryNight/StarryNight.h                                                           \
    Effects/Sunrise/Sunrise.h                                                                   \
    Effects/Swap/Swap.h                                                                         \
    Effects/SwirlCircles/SwirlCircles.h                                                         \
    Effects/SwirlCirclesAudio/SwirlCirclesAudio.h                                               \
    Effects/Visor/Visor.h                                                                       \
    Effects/Wavy/Wavy.h                                                                         \
    Effects/ZigZag/ZigZag.h                                                                     \

FORMS +=                                                                                        \
    Effects/Ambient/Ambient.ui                                                                  \
    Effects/AudioBubbles/AudioBubbles.ui                                                        \
    Effects/AudioParty/AudioParty.ui                                                            \
    Effects/AudioSine/AudioSine.ui                                                              \
    Effects/AudioStar/AudioStar.ui                                                              \
    Effects/AudioSync/AudioSync.ui                                                              \
    Effects/AudioVisualizer/AudioVisualizer.ui                                                  \
    Effects/AudioVUMeter/AudioVUMeter.ui                                                        \
    Effects/Bloom/Bloom.ui                                                                      \
    Effects/BouncingBall/BouncingBall.ui                                                        \
    Effects/Breathing/Breathing.ui                                                              \
    Effects/BreathingCircle/BreathingCircle.ui                                                  \
    Effects/Bubbles/Bubbles.ui                                                                  \
    Effects/Clock/Clock.ui                                                                      \
    Effects/ColorWheel/ColorWheel.ui                                                            \
    Effects/Comet/Comet.ui                                                                      \
    Effects/CrossingBeams/CrossingBeams.ui                                                      \
    Effects/CustomBlink/CustomBlink.ui                                                          \
    Effects/CustomGradientWave/CustomGradientWave.ui                                            \
    Effects/CustomMarquee/CustomMarquee.ui                                                      \
    Effects/DoubleRotatingRainbow/DoubleRotatingRainbow.ui                                      \
    Effects/Fill/Fill.ui                                                                        \
    Effects/FractalMotion/FractalMotion.ui                                                      \
    Effects/GifPlayer/GifPlayer.ui                                                              \
    Effects/Hypnotoad/Hypnotoad.ui                                                              \
    Effects/Layers/Layers.ui                                                                    \
    Effects/Layers/LayerEntry.ui                                                                \
    Effects/Layers/LayerGroupEntry.ui                                                           \
    Effects/Lightning/Lightning.ui                                                              \
    Effects/Marquee/Marquee.ui                                                                  \
    Effects/Mask/Mask.ui                                                                        \
    Effects/Mosaic/Mosaic.ui                                                                    \
    Effects/MotionPoint/MotionPoint.ui                                                          \
    Effects/MotionPoints/MotionPoints.ui                                                        \
    Effects/MovingPanes/MovingPanes.ui                                                          \
    Effects/NoiseMap/NoiseMap.ui                                                                \
    Effects/RadialRainbow/RadialRainbow.ui                                                      \
    Effects/Rain/Rain.ui                                                                        \
    Effects/RandomMarquee/RandomMarquee.ui                                                      \
    Effects/RandomSpin/RandomSpin.ui                                                            \
    Effects/RotatingBeam/RotatingBeam.ui                                                        \
    Effects/RotatingRainbow/RotatingRainbow.ui                                                  \
    Effects/Sequence/Sequence.ui                                                                \
    Effects/Shaders/GLSLCodeEditor.ui                                                           \
    Effects/Shaders/NewShaderPassTabHeader.ui                                                   \
    Effects/Shaders/Shaders.ui                                                                  \
    Effects/Shaders/ShaderFileTabHeader.ui                                                      \
    Effects/Shaders/ShaderPassEditor.ui                                                         \
    Effects/SmoothBlink/SmoothBlink.ui                                                          \
    Effects/SparkleFade/SparkleFade.ui                                                          \
    Effects/SpectrumCycling/SpectrumCycling.ui                                                  \
    Effects/Spiral/Spiral.ui                                                                    \
    Effects/Stack/Stack.ui                                                                      \
    Effects/StarryNight/StarryNight.ui                                                          \
    Effects/Sunrise/Sunrise.ui                                                                  \
    Effects/Swap/Swap.ui                                                                        \
    Effects/SwirlCircles/SwirlCircles.ui                                                        \
    Effects/SwirlCirclesAudio/SwirlCirclesAudio.ui                                              \
    Effects/Wavy/Wavy.ui                                                                        \
    Effects/ZigZag/ZigZag.ui                                                                    \

TRANSLATIONS +=                                                                                 \
    i18n/OpenRGB_EffectsEngine_de_DE.ts                                                         \
    i18n/OpenRGB_EffectsEngine_el_GR.ts                                                         \
    i18n/OpenRGB_EffectsEngine_en_US.ts                                                         \
    i18n/OpenRGB_EffectsEngine_en_AU.ts                                                         \
    i18n/OpenRGB_EffectsEngine_en_GB.ts                                                         \
    i18n/OpenRGB_EffectsEngine_es_ES.ts                                                         \
    i18n/OpenRGB_EffectsEngine_fr_FR.ts                                                         \
    i18n/OpenRGB_EffectsEngine_hr_HR.ts                                                         \
    i18n/OpenRGB_EffectsEngine_it_IT.ts                                                         \
    i18n/OpenRGB_EffectsEngine_ja_JP.ts                                                         \
    i18n/OpenRGB_EffectsEngine_ko_KR.ts                                                         \
    i18n/OpenRGB_EffectsEngine_ms_MY.ts                                                         \
    i18n/OpenRGB_EffectsEngine_nb_NO.ts                                                         \
    i18n/OpenRGB_EffectsEngine_pl_PL.ts                                                         \
    i18n/OpenRGB_EffectsEngine_pt_BR.ts                                                         \
    i18n/OpenRGB_EffectsEngine_ru_RU.ts                                                         \
    i18n/OpenRGB_EffectsEngine_zh_CN.ts                                                         \
    i18n/OpenRGB_EffectsEngine_zh_TW.ts                                                         \

#-----------------------------------------------------------------------------------------------#
# Windows  Configuration                                                                        #
#-----------------------------------------------------------------------------------------------#
win32:CONFIG += QTPLUGIN c++17

win32:CONFIG(debug, debug|release) {
    win32:DESTDIR = debug
}

win32:CONFIG(release, debug|release) {
    win32:DESTDIR = release
}

win32:OBJECTS_DIR = _intermediate_$$DESTDIR/.obj
win32:MOC_DIR     = _intermediate_$$DESTDIR/.moc
win32:RCC_DIR     = _intermediate_$$DESTDIR/.qrc
win32:UI_DIR      = _intermediate_$$DESTDIR/.ui

win32:contains(QMAKE_TARGET.arch, x86_64) {
    LIBS +=                                                                                     \
        -lws2_32                                                                                \
        -lole32                                                                                 \
        -lOpenGL32                                                                              \
        -luser32                                                                                \
        -lgdi32                                                                                 \
}

win32:contains(QMAKE_TARGET.arch, x86) {
    LIBS +=                                                                                     \
        -lws2_32                                                                                \
        -lole32                                                                                 \
        -lOpenGL32                                                                              \
        -luser32                                                                                \
        -lgdi32                                                                                 \
}

win32:DEFINES +=                                                                                \
    _MBCS                                                                                       \
    WIN32                                                                                       \
    _CRT_SECURE_NO_WARNINGS                                                                     \
    _WINSOCK_DEPRECATED_NO_WARNINGS                                                             \
    WIN32_LEAN_AND_MEAN                                                                         \

#-----------------------------------------------------------------------------------------------#
# Linux-specific Configuration                                                                  #
#-----------------------------------------------------------------------------------------------#
unix:!macx {
    QT += dbus
    LIBS += -lopenal -lGL -lpipewire-0.3
    QMAKE_CXXFLAGS += -std=c++17 -Wno-psabi
    target.path=$$PREFIX/lib/openrgb/plugins/
    INSTALLS += target

    INCLUDEPATH +=                                                                              \
        /usr/include/pipewire-0.3                                                               \
        /usr/include/spa-0.2                                                                    \

    INCLUDEPATH +=                                                                              \
        ScreenCapturer/linux/wayland                                                            \

    SOURCES +=                                                                                  \
        ScreenCapturer/linux/wayland/DBusScreenCastManager.cpp                                  \
        ScreenCapturer/linux/wayland/PipeWireCapturer.cpp                                       \
        ScreenCapturer/linux/wayland/SpaPodUtils.cpp                                            \
        ScreenCapturer/linux/wayland/PipeWireStreamInfo.cpp                                     \
        ScreenCapturer/linux/wayland/WaylandScreenCapturer.cpp                                  \

    HEADERS +=                                                                                  \
        ScreenCapturer/linux/wayland/DBusScreenCastManager.h                                    \
        ScreenCapturer/linux/wayland/PipeWireCapturer.h                                         \
        ScreenCapturer/linux/wayland/SpaPodUtils.h                                              \
        ScreenCapturer/linux/wayland/PipeWireStreamInfo.h                                       \
        ScreenCapturer/linux/wayland/WaylandScreenCapturer.h                                    \
}

#-----------------------------------------------------------------------------------------------#
# MacOS-specific Configuration                                                                  #
#-----------------------------------------------------------------------------------------------#
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

macx: {
    CONFIG += c++17
    LIBS += -framework OpenAL
}

RESOURCES +=                                                                                    \
    resources.qrc
