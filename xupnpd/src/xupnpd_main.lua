-- Copyright (C) 2011 Anton Burdinuk
-- clark15b@gmail.com
-- https://tsdemuxer.googlecode.com/svn/trunk/xupnpd

if cfg.daemon==true then core.detach() end

core.openlog(cfg.log_ident,cfg.log_facility)

if cfg.daemon==true then core.touchpid(cfg.pid_file) end

if cfg.embedded==true then cfg.debug=0 end

function clone_table(t)
    local tt={}
    for i,j in pairs(t) do
        tt[i]=j
    end
    return tt
end


update_id=1             -- system update_id

subscr={}               -- event sessions (for UPnP notify engine)
plugins={}              -- external plugins (YouTube, Vimeo ...)
cache={}                -- real URL cache for plugins

dofile('xupnpd_vimeo.lua')
dofile('xupnpd_youtube.lua')
dofile('xupnpd_mime.lua')
dofile('xupnpd_m3u.lua')
dofile('xupnpd_ssdp.lua')
dofile('xupnpd_http.lua')


-- download feeds from external sources (child process)
function update_feeds_async()
    local num=0
    for i,j in ipairs(feeds) do
        local plugin=plugins[ j[1] ]
        if plugin and plugin.updatefeed then
            if plugin.updatefeed(j[2],j[3])==true then num=num+1 end
        end
    end

    if num>0 then core.sendevent('reload') end

end

-- spawn child process for feeds downloading
function update_feeds(what,sec)
    core.fspawn(update_feeds_async)
    core.timer(cfg.feeds_update_interval,what)
end


-- subscribe player for ContentDirectory events
function subscribe(event,sid,callback,ttl)
    local s={}
    subscr[sid]=s

    s.event=event
    s.sid=sid
    s.callback=callback
    s.timestamp=os.time()
    s.ttl=tonumber(ttl)
    s.seq=0

    if cfg.debug>0 then print('subscribe: '..sid..', '..event..', '..callback) end

end

-- unsubscribe player
function unsubscribe(sid)
    if subscr[sid] then
        subscr[sid]=nil

        if cfg.debug>0 then print('unsubscribe: '..sid) end
    end
end


-- garbage collection
function sys_gc(what,sec)

    local t=os.time()

    -- force unsubscribe
    local g={}

    for i,j in pairs(subscr) do
        if os.difftime(t,j.timestamp)>=j.ttl then
            table.insert(g,i)
        end
    end

    for i,j in ipairs(g) do
        subscr[j]=nil

        if cfg.debug>0 then print('force unsubscribe (timeout): '..j) end
    end

    -- cache clear
    g={}

    for i,j in pairs(cache) do
        if os.difftime(t,j.time)>=cfg.cache_ttl then
            table.insert(g,i)
        end
    end

    for i,j in ipairs(g) do
        cache[j]=nil

        if cfg.debug>0 then print('remove URL from cache (timeout): '..j) end
    end

    core.timer(sec,what)
end


-- ContentDirectory event deliver (child process)
function subscr_notify_async(t)

    local tt={}
    table.insert(tt,'0,'..update_id)

    for i,j in ipairs(playlist_data.elements) do
        table.insert(tt,j.objid..','..update_id)
    end

    local data=string.format(
        '<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\"><e:property><SystemUpdateID>%s</SystemUpdateID><ContainerUpdateIDs>%s</ContainerUpdateIDs></e:property></e:propertyset>',
        update_id,table.concat(tt,','))

    for i,j in ipairs(t) do
        if cfg.debug>0 then print('notify: '..j.callback..', sid='..j.sid..', seq='..j.seq) end
        http.notify(j.callback,j.sid,data,j.seq)
    end
end


-- reload all playlists
function reload_playlist()
    reload_playlists()
    update_id=update_id+1

    if update_id>100000 then update_id=1 end

    if cfg.debug>0 then print('reload playlist, update_id='..update_id) end

    if cfg.dlna_notify==true then
        local t={}

        for i,j in pairs(subscr) do
            if j.event=='cds_event' then
                table.insert(t, { ['callback']=j.callback, ['sid']=j.sid, ['seq']=j.seq } )
                j.seq=j.seq+1
                if j.seq>100000 then j.seq=0 end
            end
        end

        if table.maxn(t)>0 then
            core.fspawn(subscr_notify_async,t)
        end
    end
end

-- change child process status (for UI)
function set_child_status(pid,status)
    pid=tonumber(pid)
    if childs[pid] then
        childs[pid].status=status
        childs[pid].time=os.time()
    end
end

-- event handlers
events['SIGUSR1']=reload_playlist
events['reload']=reload_playlist
events['store']=function(k,v) local t={} t.time=os.time() t.value=v cache[k]=t end
events['sys_gc']=sys_gc
events['subscribe']=subscribe
events['unsubscribe']=unsubscribe
events['update_feeds']=update_feeds
events['status']=set_child_status


if cfg.embedded==true then print=function () end end

print("start "..cfg.log_ident)

-- start garbage collection system
core.timer(300,'sys_gc')

http.timeout(cfg.http_timeout)

-- start feeds update system
if cfg.feeds_update_interval>0 then
    core.timer(5,'update_feeds')
end

core.mainloop()

print("stop "..cfg.log_ident)

if cfg.daemon==true then os.execute('rm -f '..cfg.pid_file) end
