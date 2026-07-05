# harbour-casts application profile

include /etc/sailjail/permissions/Base.permission
include /etc/sailjail/permissions/Internet.permission
include /etc/sailjail/permissions/UserDirs.permission
include /etc/sailjail/permissions/Downloads.permission
include /etc/sailjail/permissions/Audio.permission
include /etc/sailjail/permissions/Compatibility.permission

# Podcast database, downloads, and cached artwork.
noblacklist @{HOME}/.local/share/harbour-casts
noblacklist @{HOME}/.cache/harbour-casts
noblacklist @{HOME}/Downloads/Casts

# Session audio playback.
dbus-user.talk org.pulseaudio.Server
dbus-user.talk org.PulseAudio1
dbus-user.broadcast org.pulseaudio.Server=org.pulseaudio.*@/*
dbus-user.broadcast org.PulseAudio1=org.PulseAudio.*@/*
