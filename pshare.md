# Introduction #

pshare UPnP Playlist Browser

This program is a light DLNA Media Server which provides ContentDirectory:1 service for sharing IPTV unicast streams over local area network (with [udpxy](http://sourceforge.net/projects/udpxy) for multicast to HTTP unicast conversion).

Copyright (C) 2010 Anton Burdinuk

clark15b@gmail.com<br>
<a href='http://ps3muxer.org/pshare.html'>http://ps3muxer.org/pshare.html</a><br>
<a href='http://code.google.com/p/tsdemuxer/downloads/list'>http://code.google.com/p/tsdemuxer/downloads/list</a><br>

<a href='http://www.youtube.com/watch?feature=player_embedded&v=yBqF-WKL8tk' target='_blank'><img src='http://img.youtube.com/vi/yBqF-WKL8tk/0.jpg' width='425' height=344 /></a><br>
<br>
<h1>Usage</h1>

<pre><code>./pshare [-v] [-l] [-x] [-e] -i iface [-u device_uuid] [-t mcast_ttl] [-p http_port] [-r www_root] [playlist]<br>
   -x          XBox 360 compatible mode<br>
   -e          DLNA protocolInfo extend (DLNA profiles)<br>
   -v          Turn on verbose output<br>
   -d          Turn on verbose output + debug messages<br>
   -l          Turn on loopback multicast transmission<br>
   -i          Multicast interface address or device name<br>
   -u          DLNA server UUID<br>
   -n          DLNA server friendly name<br>
   -t          Multicast datagrams time-to-live (TTL)<br>
   -p          TCP port for incoming HTTP connections<br>
   -r          WWW root directory (default: '/opt/share/pshare/www')<br>
    playlist   single file or directory absolute path with utf8-encoded *.m3u files (default: '/opt/share/pshare/playlists')<br>
<br>
example 1: './pshare -i eth0 /opt/share/pshare/playlists/playlist.m3u'<br>
example 2: './pshare -v -i 192.168.1.1 -u 32ccc90a-27a7-494a-a02d-71f8e02b1937 -n IPTV -t 1 -p 4044 /opt/share/pshare/playlists/'<br>
<br>
known files: mpg,mpeg,mpeg2,m2v,ts,m2ts,mts,vob,avi,asf,wmv,mp4,mov,aac,ac3,mp3,ogg,wma<br>
</code></pre>

<h1>Playlist example</h1>
<pre><code>#EXTM3U<br>
#EXTINF:0,Channel 1 - TV<br>
#EXTLOGO:http://host/logo.jpg<br>
http://192.168.1.1:4022/udp/234.5.2.1:20000<br>
#EXTINF:0,Channel 2 - Radio<br>
#EXTLOGO:http://host/logo.gif<br>
#EXTTYPE:mp3,DLNA.ORG_PN=MP3;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000<br>
http://192.168.1.1:4022/udp/234.5.2.2:20000<br>
#EXTINF:0,Channel 3 - Radio<br>
http://192.168.1.1:4022/udp/234.5.2.3:20000/stream.mp3<br>
</code></pre>

<h1>Tested</h1>
<ul><li>Ubuntu 10.04 (Linux, IA-32) as Media Server<br>
</li><li>D-Link DIR-320 (DD-WRT v24 preSP2 13064, mipsel) as Media Server<br>
</li><li>ASUS WL-500gP as Media Server<br>
</li><li>Sony PlayStation 3 as UPnP player<br>
</li><li>IconBit HDS4L as UPnP player<br>
</li><li>Microsoft Media Player 11 as UPnP player<br>
</li><li>Ubuntu 10.04 with VideoLAN as UPnP player</li></ul>

<h1>ChangeLog</h1>

<b>0.0.2</b>
<ul><li>Microsoft Media Player 12 as client supported:<br>
<ul><li>X_MS_MediaReceiverRegistrar:1<br>
</li><li>ConnectionManager:1<br>
</li><li>ContentDirectory:1#GetSortCapabilities<br>
</li><li>ContentDirectory:1#Search<br>
</li><li>SUBSCRIBE, UNSUBSCRIBE<br>
</li></ul></li><li>Artist name, actor and track number<br>
</li><li>Playlists reload (SIGUSR1 or '<a href='http://host:port/reload'>http://host:port/reload</a>')<br>
</li><li>'#EXTLOGO:' for stream logo (JPEG for PS3). Examples: '#EXTLOGO: /def_logo.jpg' or '#EXTLOGO: <img src='http://host/def_logo.jpg' />'<br>
</li><li>'#EXTTYPE:' for force stream type selection: mpeg,mpeg2,ts,vob,avi,asf,wmv,mp4,mov,aac,ac3,mp3,ogg,wma<br>
</li><li>'#EXTTYPE:' optional DLNA profile after file type, example: '#EXTTYPE:mp3,DLNA.ORG_PN=MP3;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000'<br>
</li><li>No images sharing now<br>
</li><li>Ignore track length from playlist<br>
</li><li>'-O2'<br>
</li><li>HTTP proxy for Internet radio (-DWITH_PROXY, 'PS3 - transferMode.dlna.org: Streaming')<br>
</li><li>'-e' for DLNA protocolInfo extend (DLNA_ORG.PN...), needed for radio on PS3<br>
</li><li>SD (MPEG2), 720p (MPEG2), 1080i (H.264/AVC) tested on Windows Media Player and PS3<br>
</li><li>MP3 Internet-radio tested on Windows Media Player and IconBit HDS4L<br>
</li><li>Bug fixes:<br>
<ul><li>ulibc fstat() bugfix<br>
</li><li>uuid from /dev/urandom (-DWITH_URANDOM)<br>
</li><li>XML escape URLs<br>
</li><li>SystemUpdateID increment when playlists reload<br>
</li><li>'-i' now required<br>
</li><li>playlists path must be absolute<br>
</li><li>dlna:profileID="JPEG_TN" to upnp:albumArtURI for JPEG (use only JPEG for PS3)<br>
</li><li><res size="0" ...><br>
</li><li>'EXT:' header to http responses<br>
</li><li>setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,...)<br>
</li><li>trim playlist items<br>
</li><li>ContainerID in Search<br>
</li></ul></li><li>XBox360 compatible (-x) - Windows Media Connect as Twonky (fake Windows Media Player)<br>
<ul><li>start object id=100 (playlist_items_offset)<br>
</li><li>dev.xml => wmc.xml<br>
</li><li>object.container => object.container.storageFolder for child containers on XBox 360<br>
</li><li>ContainerID or ObjectID in Browse</li></ul></li></ul>

<b>0.0.1</b>
<ul><li>Sony PlayStation 3, IconBit HDS4L and VideoLAN as Media Player supported<br>
</li><li>Ubuntu 10.04 (Linux, IA-32), D-Link DIR-320 (DD-WRT v24 preSP2 13064, mipsel) and ASUS WL-500gP as Media Server supported<br>
</li><li>UTF8 encoded M3U playlists supported