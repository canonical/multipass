/// The colors used throughout the GUI, grouped by purpose.
///
/// Widgets read them through [AppColorsContext.colors] (`context.colors`)
/// instead of hardcoding values, allowing different themes to provide
/// different values.
library;

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:re_highlight/styles/atom-one-light.dart';
import 'package:xterm/xterm.dart';

extension AppColorsContext on BuildContext {
  /// The [AppColors] of the current theme, defaulting to [AppColors.light]
  /// when the theme does not provide any (e.g. in tests).
  AppColors get colors =>
      Theme.of(this).extension<AppColors>() ?? AppColors.light;
}

@immutable
class AppColors extends ThemeExtension<AppColors> {
  /// The colors of the light theme.
  static const light = AppColors(
    // General
    background: Colors.white,
    surface: Colors.white,
    textPrimary: Colors.black,
    textSelection: Colors.grey,
    inputFill: Color(0xfff2f2f2),
    inputBorder: Colors.black,
    disabledBorder: Colors.black12,
    divider: Colors.black38,
    subtleDivider: Color(0x4d9e9e9e),
    tableBorder: Colors.grey,
    cardBorder: Color(0xff707070),
    cardDropdownFill: Color(0xfff5f5f5),
    bannerBackground: Color(0xfff7f7f7),
    tooltipBackground: Color(0xff111111),
    shadow: Colors.black54,
    notificationShadow: Colors.black38,
    scrim: Colors.black54,
    emptyStateIcon: Color(0xffd9d9d9),
    iconSecondary: Color(0xff333333),
    // Buttons and tabs
    buttonBorder: Color(0xff333333),
    positive: Color(0xff0e8620),
    onPositive: Colors.white,
    negative: Color(0xffc7162b),
    onNegative: Colors.white,
    toggleSelectedBackground: Color(0xff333333),
    toggleSelectedForeground: Colors.white,
    tabIndicator: Colors.black12,
    // Sliders, switches and charts
    sliderActiveTrack: Color(0xff0066cc),
    sliderInactiveTrack: Color(0xffd9d9d9),
    sliderThumb: Colors.white,
    sliderThumbBorder: Color(0xff757575),
    switchActiveTrack: CupertinoColors.activeBlue,
    switchInactiveTrack: Color(0xffd9d9d9),
    usageNormal: Color(0xff0066cc),
    usageHigh: Color(0xffec6c04),
    usageTrack: Color(0x3d006ada),
    sparklineLine: Color(0xff666666),
    sparklineFill: Color(0xff717171),
    // Links
    link: Colors.blue,
    linkSecondary: Color(0xff1976d2),
    // Feedback
    info: Colors.blue,
    success: Colors.green,
    warning: Color(0xffcc7900),
    error: Colors.red,
    loadingIndicator: Colors.orange,
    // Instance states
    statusRunning: Color(0xff0c8420),
    statusStopped: Colors.black,
    statusSuspended: Color(0xff666666),
    statusTransitioning: Color(0xff757575),
    statusUnknown: Color(0xff757575),
    statusDeleted: Colors.redAccent,
    // Brand
    brand: Color(0xffe95420),
    onBrand: Colors.white,
    // Sidebar
    sidebarBackground: Color(0xff262626),
    sidebarForeground: Colors.white,
    sidebarHover: Colors.white24,
    sidebarDivider: Color(0x4dffffff),
    sidebarSelectedBackground: Color(0xff444444),
    sidebarBadgeBackground: Color(0xff333333),
    // Terminal
    terminalTabBarBackground: Color(0xff222222),
    terminalTabSelectedBackground: Color(0xff2b2b2b),
    terminalTabForeground: Colors.white,
    terminalTabHover: Colors.white24,
    terminal: TerminalTheme(
      cursor: Color(0xffe5e5e5),
      selection: Color(0x80e5e5e5),
      foreground: Color(0xffffffff),
      background: Color(0xff380c2a),
      black: Color(0xff000000),
      white: Color(0xffe5e5e5),
      red: Color(0xffcd3131),
      green: Color(0xff0dbc79),
      yellow: Color(0xffe5e510),
      blue: Color(0xff2472c8),
      magenta: Color(0xffbc3fbc),
      cyan: Color(0xff11a8cd),
      brightBlack: Color(0xff666666),
      brightRed: Color(0xfff14c4c),
      brightGreen: Color(0xff23d18b),
      brightYellow: Color(0xfff5f543),
      brightBlue: Color(0xff3b8eea),
      brightMagenta: Color(0xffd670d6),
      brightCyan: Color(0xff29b8db),
      brightWhite: Color(0xffffffff),
      searchHitBackground: Color(0xffffff2b),
      searchHitBackgroundCurrent: Color(0xff31ff26),
      searchHitForeground: Color(0xff000000),
    ),
    // Code editor
    codeHighlight: atomOneLightTheme,
  );

  const AppColors({
    required this.background,
    required this.surface,
    required this.textPrimary,
    required this.textSelection,
    required this.inputFill,
    required this.inputBorder,
    required this.disabledBorder,
    required this.divider,
    required this.subtleDivider,
    required this.tableBorder,
    required this.cardBorder,
    required this.cardDropdownFill,
    required this.bannerBackground,
    required this.tooltipBackground,
    required this.shadow,
    required this.notificationShadow,
    required this.scrim,
    required this.emptyStateIcon,
    required this.iconSecondary,
    required this.buttonBorder,
    required this.positive,
    required this.onPositive,
    required this.negative,
    required this.onNegative,
    required this.toggleSelectedBackground,
    required this.toggleSelectedForeground,
    required this.tabIndicator,
    required this.sliderActiveTrack,
    required this.sliderInactiveTrack,
    required this.sliderThumb,
    required this.sliderThumbBorder,
    required this.switchActiveTrack,
    required this.switchInactiveTrack,
    required this.usageNormal,
    required this.usageHigh,
    required this.usageTrack,
    required this.sparklineLine,
    required this.sparklineFill,
    required this.link,
    required this.linkSecondary,
    required this.info,
    required this.success,
    required this.warning,
    required this.error,
    required this.loadingIndicator,
    required this.statusRunning,
    required this.statusStopped,
    required this.statusSuspended,
    required this.statusTransitioning,
    required this.statusUnknown,
    required this.statusDeleted,
    required this.brand,
    required this.onBrand,
    required this.sidebarBackground,
    required this.sidebarForeground,
    required this.sidebarHover,
    required this.sidebarDivider,
    required this.sidebarSelectedBackground,
    required this.sidebarBadgeBackground,
    required this.terminalTabBarBackground,
    required this.terminalTabSelectedBackground,
    required this.terminalTabForeground,
    required this.terminalTabHover,
    required this.terminal,
    required this.codeHighlight,
  });

  // General

  /// Background of the main window.
  final Color background;

  /// Background of panels, dialogs, notifications and the launch form.
  final Color surface;

  /// Default color of text and icons on [background] and [surface].
  final Color textPrimary;

  /// Highlight behind selected text.
  final Color textSelection;

  /// Background of text fields and dropdowns.
  final Color inputFill;

  /// Underline of text fields and dropdowns.
  final Color inputBorder;

  /// Border of disabled text fields.
  final Color disabledBorder;

  /// Separator between list entries, such as mount points.
  final Color divider;

  /// Fainter separator, such as between zones in the zones popup.
  final Color subtleDivider;

  /// Borders of the instances table and its column selector.
  final Color tableBorder;

  /// Border of catalogue image cards.
  final Color cardBorder;

  /// Background of the release dropdown in catalogue image cards.
  final Color cardDropdownFill;

  /// Background of the "update available" banner.
  final Color bannerBackground;

  /// Background of tooltips.
  final Color tooltipBackground;

  /// Shadow around dialogs.
  final Color shadow;

  /// Shadow around notifications.
  final Color notificationShadow;

  /// Overlay that dims the app while the daemon is unavailable.
  final Color scrim;

  /// Logo shown when there are no instances.
  final Color emptyStateIcon;

  /// Icons inside secondary buttons, such as the table column selector.
  final Color iconSecondary;

  // Buttons and tabs

  /// Border of outlined (secondary) buttons.
  final Color buttonBorder;

  /// Background of primary buttons and color of "save" actions.
  final Color positive;

  /// Text and icons on [positive].
  final Color onPositive;

  /// Background of destructive buttons and color of "discard" actions.
  final Color negative;

  /// Text and icons on [negative].
  final Color onNegative;

  /// Background of the selected button in a toggle group.
  final Color toggleSelectedBackground;

  /// Text of the selected button in a toggle group.
  final Color toggleSelectedForeground;

  /// Fill behind the selected tab.
  final Color tabIndicator;

  // Sliders, switches and charts

  /// Slider track to the left of the thumb.
  final Color sliderActiveTrack;

  /// Slider track to the right of the thumb.
  final Color sliderInactiveTrack;

  /// Fill of the slider thumb.
  final Color sliderThumb;

  /// Outline of the slider thumb.
  final Color sliderThumbBorder;

  /// Track of a switch that is on.
  final Color switchActiveTrack;

  /// Track of a switch that is off.
  final Color switchInactiveTrack;

  /// Memory and disk usage bars below 80%.
  final Color usageNormal;

  /// Memory and disk usage bars at or above 80%.
  final Color usageHigh;

  /// Unfilled part of memory and disk usage bars.
  final Color usageTrack;

  /// Line of the CPU usage sparkline.
  final Color sparklineLine;

  /// Area under the CPU usage sparkline.
  final Color sparklineFill;

  // Links

  /// Text links.
  final Color link;

  /// Inline actions and info icons, such as "Enable zone".
  final Color linkSecondary;

  // Feedback

  /// In-progress notifications.
  final Color info;

  /// Successful notifications.
  final Color success;

  /// Warning notifications and icons.
  final Color warning;

  /// Error notifications and messages.
  final Color error;

  /// Spinner shown while waiting for the daemon.
  final Color loadingIndicator;

  // Instance states

  /// Running and delayed-shutdown instances.
  final Color statusRunning;

  /// Stopped instances.
  final Color statusStopped;

  /// Suspended instances.
  final Color statusSuspended;

  /// Starting, restarting and suspending instances.
  final Color statusTransitioning;

  /// Unavailable instances and unknown states.
  final Color statusUnknown;

  /// Deleted instances.
  final Color statusDeleted;

  // Brand

  /// Ubuntu orange, used for logos and highlights.
  final Color brand;

  /// Text and icons on [brand].
  final Color onBrand;

  // Sidebar

  /// Background of the sidebar.
  final Color sidebarBackground;

  /// Text, icons and selection marker in the sidebar.
  final Color sidebarForeground;

  /// Highlight of hovered sidebar buttons.
  final Color sidebarHover;

  /// Separator in the sidebar.
  final Color sidebarDivider;

  /// Background of the selected sidebar entry.
  final Color sidebarSelectedBackground;

  /// Background of the instance count badge in the sidebar.
  final Color sidebarBadgeBackground;

  // Terminal

  /// Background of the terminal tab bar and unselected tabs.
  final Color terminalTabBarBackground;

  /// Background of the selected terminal tab.
  final Color terminalTabSelectedBackground;

  /// Text and icons in the terminal tab bar.
  final Color terminalTabForeground;

  /// Highlight of hovered terminal tab buttons.
  final Color terminalTabHover;

  /// Colors of the terminal itself, including its ANSI palette.
  final TerminalTheme terminal;

  // Code editor

  /// Syntax highlighting of the cloud-init editor.
  final Map<String, TextStyle> codeHighlight;

  @override
  AppColors copyWith({
    Color? background,
    Color? surface,
    Color? textPrimary,
    Color? textSelection,
    Color? inputFill,
    Color? inputBorder,
    Color? disabledBorder,
    Color? divider,
    Color? subtleDivider,
    Color? tableBorder,
    Color? cardBorder,
    Color? cardDropdownFill,
    Color? bannerBackground,
    Color? tooltipBackground,
    Color? shadow,
    Color? notificationShadow,
    Color? scrim,
    Color? emptyStateIcon,
    Color? iconSecondary,
    Color? buttonBorder,
    Color? positive,
    Color? onPositive,
    Color? negative,
    Color? onNegative,
    Color? toggleSelectedBackground,
    Color? toggleSelectedForeground,
    Color? tabIndicator,
    Color? sliderActiveTrack,
    Color? sliderInactiveTrack,
    Color? sliderThumb,
    Color? sliderThumbBorder,
    Color? switchActiveTrack,
    Color? switchInactiveTrack,
    Color? usageNormal,
    Color? usageHigh,
    Color? usageTrack,
    Color? sparklineLine,
    Color? sparklineFill,
    Color? link,
    Color? linkSecondary,
    Color? info,
    Color? success,
    Color? warning,
    Color? error,
    Color? loadingIndicator,
    Color? statusRunning,
    Color? statusStopped,
    Color? statusSuspended,
    Color? statusTransitioning,
    Color? statusUnknown,
    Color? statusDeleted,
    Color? brand,
    Color? onBrand,
    Color? sidebarBackground,
    Color? sidebarForeground,
    Color? sidebarHover,
    Color? sidebarDivider,
    Color? sidebarSelectedBackground,
    Color? sidebarBadgeBackground,
    Color? terminalTabBarBackground,
    Color? terminalTabSelectedBackground,
    Color? terminalTabForeground,
    Color? terminalTabHover,
    TerminalTheme? terminal,
    Map<String, TextStyle>? codeHighlight,
  }) {
    return AppColors(
      background: background ?? this.background,
      surface: surface ?? this.surface,
      textPrimary: textPrimary ?? this.textPrimary,
      textSelection: textSelection ?? this.textSelection,
      inputFill: inputFill ?? this.inputFill,
      inputBorder: inputBorder ?? this.inputBorder,
      disabledBorder: disabledBorder ?? this.disabledBorder,
      divider: divider ?? this.divider,
      subtleDivider: subtleDivider ?? this.subtleDivider,
      tableBorder: tableBorder ?? this.tableBorder,
      cardBorder: cardBorder ?? this.cardBorder,
      cardDropdownFill: cardDropdownFill ?? this.cardDropdownFill,
      bannerBackground: bannerBackground ?? this.bannerBackground,
      tooltipBackground: tooltipBackground ?? this.tooltipBackground,
      shadow: shadow ?? this.shadow,
      notificationShadow: notificationShadow ?? this.notificationShadow,
      scrim: scrim ?? this.scrim,
      emptyStateIcon: emptyStateIcon ?? this.emptyStateIcon,
      iconSecondary: iconSecondary ?? this.iconSecondary,
      buttonBorder: buttonBorder ?? this.buttonBorder,
      positive: positive ?? this.positive,
      onPositive: onPositive ?? this.onPositive,
      negative: negative ?? this.negative,
      onNegative: onNegative ?? this.onNegative,
      toggleSelectedBackground:
          toggleSelectedBackground ?? this.toggleSelectedBackground,
      toggleSelectedForeground:
          toggleSelectedForeground ?? this.toggleSelectedForeground,
      tabIndicator: tabIndicator ?? this.tabIndicator,
      sliderActiveTrack: sliderActiveTrack ?? this.sliderActiveTrack,
      sliderInactiveTrack: sliderInactiveTrack ?? this.sliderInactiveTrack,
      sliderThumb: sliderThumb ?? this.sliderThumb,
      sliderThumbBorder: sliderThumbBorder ?? this.sliderThumbBorder,
      switchActiveTrack: switchActiveTrack ?? this.switchActiveTrack,
      switchInactiveTrack: switchInactiveTrack ?? this.switchInactiveTrack,
      usageNormal: usageNormal ?? this.usageNormal,
      usageHigh: usageHigh ?? this.usageHigh,
      usageTrack: usageTrack ?? this.usageTrack,
      sparklineLine: sparklineLine ?? this.sparklineLine,
      sparklineFill: sparklineFill ?? this.sparklineFill,
      link: link ?? this.link,
      linkSecondary: linkSecondary ?? this.linkSecondary,
      info: info ?? this.info,
      success: success ?? this.success,
      warning: warning ?? this.warning,
      error: error ?? this.error,
      loadingIndicator: loadingIndicator ?? this.loadingIndicator,
      statusRunning: statusRunning ?? this.statusRunning,
      statusStopped: statusStopped ?? this.statusStopped,
      statusSuspended: statusSuspended ?? this.statusSuspended,
      statusTransitioning: statusTransitioning ?? this.statusTransitioning,
      statusUnknown: statusUnknown ?? this.statusUnknown,
      statusDeleted: statusDeleted ?? this.statusDeleted,
      brand: brand ?? this.brand,
      onBrand: onBrand ?? this.onBrand,
      sidebarBackground: sidebarBackground ?? this.sidebarBackground,
      sidebarForeground: sidebarForeground ?? this.sidebarForeground,
      sidebarHover: sidebarHover ?? this.sidebarHover,
      sidebarDivider: sidebarDivider ?? this.sidebarDivider,
      sidebarSelectedBackground:
          sidebarSelectedBackground ?? this.sidebarSelectedBackground,
      sidebarBadgeBackground:
          sidebarBadgeBackground ?? this.sidebarBadgeBackground,
      terminalTabBarBackground:
          terminalTabBarBackground ?? this.terminalTabBarBackground,
      terminalTabSelectedBackground:
          terminalTabSelectedBackground ?? this.terminalTabSelectedBackground,
      terminalTabForeground:
          terminalTabForeground ?? this.terminalTabForeground,
      terminalTabHover: terminalTabHover ?? this.terminalTabHover,
      terminal: terminal ?? this.terminal,
      codeHighlight: codeHighlight ?? this.codeHighlight,
    );
  }

  @override
  AppColors lerp(AppColors? other, double t) {
    if (other == null) return this;
    Color c(Color a, Color b) => Color.lerp(a, b, t)!;
    return AppColors(
      background: c(background, other.background),
      surface: c(surface, other.surface),
      textPrimary: c(textPrimary, other.textPrimary),
      textSelection: c(textSelection, other.textSelection),
      inputFill: c(inputFill, other.inputFill),
      inputBorder: c(inputBorder, other.inputBorder),
      disabledBorder: c(disabledBorder, other.disabledBorder),
      divider: c(divider, other.divider),
      subtleDivider: c(subtleDivider, other.subtleDivider),
      tableBorder: c(tableBorder, other.tableBorder),
      cardBorder: c(cardBorder, other.cardBorder),
      cardDropdownFill: c(cardDropdownFill, other.cardDropdownFill),
      bannerBackground: c(bannerBackground, other.bannerBackground),
      tooltipBackground: c(tooltipBackground, other.tooltipBackground),
      shadow: c(shadow, other.shadow),
      notificationShadow: c(notificationShadow, other.notificationShadow),
      scrim: c(scrim, other.scrim),
      emptyStateIcon: c(emptyStateIcon, other.emptyStateIcon),
      iconSecondary: c(iconSecondary, other.iconSecondary),
      buttonBorder: c(buttonBorder, other.buttonBorder),
      positive: c(positive, other.positive),
      onPositive: c(onPositive, other.onPositive),
      negative: c(negative, other.negative),
      onNegative: c(onNegative, other.onNegative),
      toggleSelectedBackground: c(
        toggleSelectedBackground,
        other.toggleSelectedBackground,
      ),
      toggleSelectedForeground: c(
        toggleSelectedForeground,
        other.toggleSelectedForeground,
      ),
      tabIndicator: c(tabIndicator, other.tabIndicator),
      sliderActiveTrack: c(sliderActiveTrack, other.sliderActiveTrack),
      sliderInactiveTrack: c(sliderInactiveTrack, other.sliderInactiveTrack),
      sliderThumb: c(sliderThumb, other.sliderThumb),
      sliderThumbBorder: c(sliderThumbBorder, other.sliderThumbBorder),
      switchActiveTrack: c(switchActiveTrack, other.switchActiveTrack),
      switchInactiveTrack: c(switchInactiveTrack, other.switchInactiveTrack),
      usageNormal: c(usageNormal, other.usageNormal),
      usageHigh: c(usageHigh, other.usageHigh),
      usageTrack: c(usageTrack, other.usageTrack),
      sparklineLine: c(sparklineLine, other.sparklineLine),
      sparklineFill: c(sparklineFill, other.sparklineFill),
      link: c(link, other.link),
      linkSecondary: c(linkSecondary, other.linkSecondary),
      info: c(info, other.info),
      success: c(success, other.success),
      warning: c(warning, other.warning),
      error: c(error, other.error),
      loadingIndicator: c(loadingIndicator, other.loadingIndicator),
      statusRunning: c(statusRunning, other.statusRunning),
      statusStopped: c(statusStopped, other.statusStopped),
      statusSuspended: c(statusSuspended, other.statusSuspended),
      statusTransitioning: c(statusTransitioning, other.statusTransitioning),
      statusUnknown: c(statusUnknown, other.statusUnknown),
      statusDeleted: c(statusDeleted, other.statusDeleted),
      brand: c(brand, other.brand),
      onBrand: c(onBrand, other.onBrand),
      sidebarBackground: c(sidebarBackground, other.sidebarBackground),
      sidebarForeground: c(sidebarForeground, other.sidebarForeground),
      sidebarHover: c(sidebarHover, other.sidebarHover),
      sidebarDivider: c(sidebarDivider, other.sidebarDivider),
      sidebarSelectedBackground: c(
        sidebarSelectedBackground,
        other.sidebarSelectedBackground,
      ),
      sidebarBadgeBackground: c(
        sidebarBadgeBackground,
        other.sidebarBadgeBackground,
      ),
      terminalTabBarBackground: c(
        terminalTabBarBackground,
        other.terminalTabBarBackground,
      ),
      terminalTabSelectedBackground: c(
        terminalTabSelectedBackground,
        other.terminalTabSelectedBackground,
      ),
      terminalTabForeground: c(
        terminalTabForeground,
        other.terminalTabForeground,
      ),
      terminalTabHover: c(terminalTabHover, other.terminalTabHover),
      // These can't be interpolated, so they switch halfway through.
      terminal: t < 0.5 ? terminal : other.terminal,
      codeHighlight: t < 0.5 ? codeHighlight : other.codeHighlight,
    );
  }
}
