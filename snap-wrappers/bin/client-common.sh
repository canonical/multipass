# so that snapd finds autostart desktop files where it expects them (see https://snapcraft.io/docs/snap-format)
link_autostart()
{
  if [ ! -z "$SNAP_USER_DATA" ]  # ensure we're in a proper environment
  then
    mkdir -p $SNAP_USER_DATA/.config
    ln -sfnt $SNAP_USER_DATA/.config ../config/autostart
  else
    echo "WARNING: SNAP_USER_DATA empty"
  fi
}
