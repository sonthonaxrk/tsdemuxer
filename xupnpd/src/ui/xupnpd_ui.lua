ui_args=nil
ui_data=nil

function ui_main()
    http.sendtfile(cfg.ui_path..'ui_main.html',http_vars)
end

function ui_error()
    http.send('<h2>Error occurred</h3>')
    http.send('<br/><a class="btn btn-info" href="/ui">Back</a>')
end

function ui_downloads()
    http.send('<h3>Downloads</h3>')
    http.send('<br/><table class="table">')
    for i,j in ipairs(playlist_data.elements[1].elements) do
        http.send(string.format('<tr><td><a href="/ui/%s.m3u">%s</a></td></tr>',j.name,j.name))
    end
    http.send('</table>')
    http.send('<br/><a class="btn btn-info" href="/ui">Back</a>')
end

function ui_download(name)
    name=util.urldecode(string.match(name,'(.+)%.m3u$'))

    local pls=nil

    for i,j in ipairs(playlist_data.elements[1].elements) do
        if j.name==name then pls=j break end
    end

    if not pls then
        http.send(
            string.format(
                'HTTP/1.1 404 Not found\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\n'..
                'Connection: close\r\n\r\n',os.date('!%a, %d %b %Y %H:%M:%S GMT'),ssdp_server)
        )
        return
    end

    http.send(
        string.format(
            'HTTP/1.1 200 Ok\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: none\r\n'..
            'Connection: close\r\nContent-Type: audio/x-mpegurl\r\n\r\n',os.date('!%a, %d %b %Y %H:%M:%S GMT'),ssdp_server)
    )

    http.send('#EXTM3U\n')
    for i,j in ipairs(pls.elements) do
        local url=j.url or ''
        if j.path then
            url=www_location..'/stream?s='..util.urlencode(j.objid)
        else
            if cfg.proxy>0 then
                if cfg.proxy>1 or pls.mime[1]==2 then
                    url=www_location..'/proxy?s='..util.urlencode(j.objid)
                end
            end
        end
        http.send('#EXTINF:0,'..j.name..'\n'..url..'\n')
    end
end

function ui_playlists()
    http.send('<h3>Playlists</h3>')
    http.send('<br/><table class="table">')

    local d=util.dir(cfg.playlists_path)
    if d then
        table.sort(d)
        for i,j in ipairs(d) do
            if string.find(j,'.+%.m3u$') then
                local fname=util.urlencode(j)
                http.send(string.format('<tr><td><a href=\'/ui/show?fname=%s\'>%s</a> [<a href=\'/ui/remove?fname=%s\'>x</a>]</td></tr>\n',fname,j,fname))
            end
        end
    end

    http.send('</table>')

    http.send('<br/><h3>Upload *.m3u file</h3>')
    http.send('<form method=post action="/ui/upload" enctype="multipart/form-data">')
    http.send('<input type=file name=m3ufile><br /><br />')
    http.send('<input class="btn btn-primary" type=submit value=Send>')
    http.send('</form><hr/>')
    http.send('<br/><a class="btn btn-primary" href="/ui/reload">Reload</a> <a class="btn btn-primary" href="/ui/reload_feeds?return_url=/ui/playlists">Reload feeds</a> <a class="btn btn-info" href="/ui">Back</a>')
end

function ui_feeds()
    http.send('<h3>Feeds</h3>')
    http.send('<br/><table class="table">')

    for i,j in ipairs(feeds) do
        http.send(string.format('<tr><td>%s [<a href="/ui/remove_feed?id=%s">x</a>]</td></tr>\n',j[3],i))
    end

    http.send('</table>')

    http.send('<h3>Add feed</h3>')

    http.send('<form method=get action="/ui/add_feed">')

    http.send('<div class="controls controls-row"><div class="span2">Plugin</div><select class="span4" name="plugin">')

    for plugin_name,plugin in pairs(plugins) do
        if plugin.name then
            http.send(string.format('<option value="%s">%s</option>',plugin_name,plugin.name))
        end
    end

    http.send('</select></div>')

    http.send('<div class="controls controls-row"><div class="span2">Feed data</div><input class="span4" type="text" name="feed"><div class="span1"><a href="/ui/fhelp" target="_blank">?</a></div></div>')

    http.send('<div class="controls controls-row"><div class="span2">Name</div><input class="span4" type="text" name="name"></div>')

    http.send('<br/><input class="btn btn-primary" type=submit value=Add> <a class="btn btn-info" href="/ui/fhelp" target="_blank">Help</a>')
    http.send('</form><hr/>')

    http.send('<br/><a class="btn btn-primary" href="/ui/save_feeds">Save</a> <a class="btn btn-primary" href="/ui/reload_feeds?return_url=/ui/feeds">Reload feeds</a> <a class="btn btn-info" href="/ui">Back</a>')
end

function ui_fhelp()

    http.send('<br/>')

    for plugin_name,plugin in pairs(plugins) do
        if plugin.name and plugin.desc then
            http.send(string.format('<b>%s</b>: ',plugin.name))
            http.send(plugin.desc)
            http.send('<br/><br/>')
        end
    end
end


function ui_show()
    if ui_args.fname then
        local real_name=util.urldecode(ui_args.fname)
        if string.find(real_name,'^[^-/\\]+%.m3u$') then
            local pls=m3u.parse(cfg.playlists_path..real_name)

            if pls then
                http.send('<h3>'..pls.name..'</h3>')
                http.send('<br/><table class="table">')
                for i,j in ipairs(pls.elements) do
                    http.send(string.format('<tr><td><a href="%s">%s</a></td></tr>',j.url,j.name))
                end
                http.send('</table>')
            end
        end
    end

    http.send('<br/><a class="btn btn-info" href="/ui/playlists">Back</a>')
end

function ui_remove()
    if ui_args.fname then
        local real_name=util.urldecode(ui_args.fname)
        if string.find(real_name,'^[^-/\\]+%.m3u$') then
            if os.remove(cfg.playlists_path..real_name) then
                core.sendevent('reload')
                http.send('<h3>OK</h3>')
            else
                http.send('<h3>Fail</h3>')
            end
        end
    end

    http.send('<br/><a class="btn btn-info" href="/ui/playlists">Back</a>')
end

function ui_remove_feed()
    if ui_args.id and feeds[tonumber(ui_args.id)] then
        core.sendevent('remove_feed',ui_args.id)
        http.send('<h3>OK</h3>')
    else
        http.send('<h3>Fail</h3>')
    end

    http.send('<br/><a class="btn btn-info" href="/ui/feeds">Back</a>')
end

function ui_add_feed()
    if ui_args.plugin and ui_args.feed then
        if not ui_args.name or string.len(ui_args.name)==0 then ui_args.name=string.gsub(ui_args.feed,'/',' ') end
        core.sendevent('add_feed',ui_args.plugin,ui_args.feed,ui_args.name)
        http.send('<h3>OK</h3>')
    else
        http.send('<h3>Fail</h3>')
    end

    http.send('<br/><a class="btn btn-info" href="/ui/feeds">Back</a>')
end

function save_feeds()

    local f=io.open(cfg.config_path..'feeds.lua','w')
    if f then
        f:write('feeds=\n{\n')

        for i,j in ipairs(feeds) do
            f:write(string.format('   { "%s", "%s", "%s" },\n',j[1],j[2],j[3]))
        end

        f:write('}\n')

        f:close()
        return true
    end

    return false
end


function ui_save_feeds()
    if save_feeds() then http.send('<h3>OK</h3>') else http.send('<h3>Fail</h3>') end

    http.send('<br/><a class="btn btn-info" href="/ui/feeds">Back</a>')
end

function ui_reload()
    core.sendevent('reload')
    http.send('<h3>OK</h3>')
    http.send('<br/><a class="btn btn-info" href="/ui/playlists">Back</a>')
end

function ui_reload_feeds()
    update_feeds_async()
    http.send('<h3>OK</h3>')
    http.send('<br/><a class="btn btn-info" href="'.. (ui_args.return_url or '/ui') ..'">Back</a>')
end

function ui_config()
    http_vars.youtube_fmt=cfg.youtube_fmt
    http_vars.youtube_region=cfg.youtube_region
    http_vars.ivi_fmt=cfg.ivi_fmt
    http_vars.ag_fmt=cfg.ag_fmt
    local vk_name=plugins.vkontakte.vk_get_name()
    if vk_name then
        http_vars.vk_auth_link='You are signed as <b>'..vk_name..'</b> <a class="btn btn-info" href="'..plugins.vkontakte.vk_api_request_auth(www_location)..'">sign-in as another user</a>'
    else
        http_vars.vk_auth_link='<a class="btn btn-info" href="'..plugins.vkontakte.vk_api_request_auth(www_location)..'">VKontakte sign-in</a>'
    end
    http.sendtfile(cfg.ui_path..'ui_config.html',http_vars)
end

function ui_apply()
    local f=io.open(cfg.config_path..'common.lua','w')
    if f then
        f:write('cfg.youtube_fmt="',ui_args.youtube_q or '18','"\ncfg.ivi_fmt="',ui_args.ivi_q or 'MP4-lo','"\ncfg.ag_fmt="',ui_args.ag_q or 'MP4-lo','"\ncfg.youtube_region="',ui_args.youtube_r or '*','"\n')
        f:close()
        core.sendevent('config')
    end

    http.send('<h3>OK</h3>')
    http.send('<br/><a class="btn btn-info" href="/ui/config">Back</a>')
end

function ui_vk_landing()
    http.send("<script>location.href = document.URL.replace('vk_landing', 'vk_update').replace('#', '?');</script>")
end

function ui_vk_update()
    local f=io.open(cfg.config_path..'vkontakte.lua','w')
    if f then
        if ui_args.access_token and ui_args.secret and ui_args.user_id then
            f:write('cfg.vk_api_access_token="',ui_args.access_token,'"\ncfg.vk_api_secret="',ui_args.secret,
                '"\ncfg.vk_api_user_id="',ui_args.user_id,'"\n')
            http.send('<h3>VKontakte sign-in OK</h3>')
        else
            f:write('cfg.vk_api_access_token=""\ncfg.vk_api_secret=""\ncfg.vk_api_user_id=""\n')
            http.send('<h3>VKontakte sign-in FAILED</h3>')
            http.send('Error: '..util.urldecode(ui_args.error)..'<br />Error description: '..util.urldecode(ui_args.error_description)..'<br />')
        end
        f:close()
        core.sendevent('config')
    else
        http.send('<h3>Error opening config file</h3>')
    end
    http.send('<br/><a class="btn btn-info" href="/ui/config">Back</a>')
end

function ui_vk_status()
    http.send('<h3>VKontakte Status</h3>')
    local vk_name=plugins.vkontakte.vk_get_name()
    if vk_name then
        http.send('You are signed as <b>'..vk_name..'</b> <a href="'..plugins.vkontakte.vk_api_request_auth(www_location)..'">sign-in as another user</a>')
        http.send('<h4>Groups</h4>')
        http.send('<table class="table" width="400"><tr><th width="300">group name</th><th>groip id</th></tr>')
        local vk_groups=plugins.vkontakte.vk_get_groups()
        if vk_groups then
            for group_id,group_name in pairs(vk_groups) do
                http.send(string.format('<tr><td><a href="http://vkontakte.ru/club%s">%s</a></td><td>%s</td></tr>',group_id,group_name,group_id))
            end
        end
        http.send('</table>')
        http.send('<h4>Firiends</h4>')
        http.send('<table class="table" width="400"><tr><th width="300">friend name</th><th>user id</th></tr>')
        local vk_friends=plugins.vkontakte.vk_get_friends()
        if vk_friends then
            for user_id,user_name in pairs(vk_friends) do
                http.send(string.format('<tr><td><a href="http://vkontakte.ru/id%s">%s</a></td><td>%s</td></tr>',user_id,user_name,user_id))
            end
        end
        http.send('</table>')
    else
        http.send('You are not signed in. ')
        http.send('<a href="'..plugins.vkontakte.vk_api_request_auth(www_location)..'">sign-in</a>')
    end

    http.send('<br/><br/><a class="btn btn-primary" href="/ui/vk_status">Refresh</a>')
end

function ui_status()
    http.send('<h3>Status</h3>')
    http.send('<br/><table class="table">')

    for i,j in pairs(childs) do
        if j.status then
            http.send(string.format('<tr><td>%s [<a href="/ui/kill?pid=%s">x</a>]</td></tr>',j.status,i))
        end
    end

    http.send('</table>')

    http.send('<br/><a class="btn btn-primary" href="/ui/status">Refresh</a> <a class="btn btn-info" href="/ui">Back</a>')
end

function ui_kill()
    if ui_args.pid and childs[tonumber(ui_args.pid)] then
        util.kill(ui_args.pid)
        http.send('<h3>OK</h3>')
    else
        http.send('<h3>Fail</h3>')
    end
    http.send('<br/><a class="btn btn-info" href="/ui/status">Back</a>')
end

function ui_upload()
    local tt=util.multipart_split(ui_data)
    ui_data=nil

    if tt and tt[1] then
        local n,m=string.find(tt[1],'\r?\n\r?\n')

        if n then
            local fname=string.match(string.sub(tt[1],1,n-1),'filename=\"(.+)\"')

            if fname and string.find(fname,'.+%.m3u$') then
                local tfname=cfg.tmp_path..fname

                local fd=io.open(tfname,'w+')
                if fd then
                    fd:write(string.sub(tt[1],m+1))
                    fd:close()
                end

                local pls=m3u.parse(tfname)

                if pls then
                    if os.execute(string.format('mv %s %s',tfname,cfg.playlists_path..fname))~=0 then
                        os.remove(tfname)
                        http.send('<h3>Fail</h3>')
                    else
                        core.sendevent('reload')
                        http.send('<h3>OK</h3>')
                    end
                else
                    os.remove(tfname)
                    http.send('<h3>Fail</h3>')
                end
            else
                http.send('<h3>Fail</h3>')
            end
        end
    end

    http.send('<br/><a class="btn btn-info" href="/ui/playlists">Back</a>')
end

function ui_api_call(args)
    http_send_headers(200,'txt')

    if args.action=='feeds' then
        for i,j in ipairs(feeds) do http.send(string.format('%s;%s\r\n',i,j[3])) end
    elseif args.action=='reload' then
        core.sendevent('reload')
        http.send('OK\r\n')
    elseif args.action=='add_feed' then
        core.sendevent('add_feed',args.plugin or '',args.feed or '',args.name or '')
        http.send('OK\r\n')
    elseif args.action=='remove_feed' then
        core.sendevent('remove_feed',args.id)
        http.send('OK\r\n')
    elseif args.action=='save_feeds' then
        save_feeds()
        update_feeds_async()
        http.send('OK\r\n')
    elseif args.action=='update_feeds' then
        update_feeds_async()
        http.send('OK\r\n')
    elseif args.action=='status' then
        for i,j in pairs(childs) do
            if j.status then
                http.send(string.format('%s;%s\r\n',i,j.status))
            end
        end
    elseif args.action=='kill' then
        if args.pid and childs[tonumber(args.pid)] then
            util.kill(args.pid)
            http.send('OK\r\n')
        else
            http.send('ERR\r\n')
        end
    elseif args.action=='playlists' then
        for i,j in ipairs(playlist_data.elements[1].elements) do
            http.send(string.format('%s;%s\r\n',i,j.name))
        end
    elseif args.action=='playlist' then
        local pls=playlist_data.elements[1].elements[tonumber(args.id)]
        if pls then
            for i,j in ipairs(pls.elements) do
                http.send(string.format('%s;%s;%s\r\n',i,j.logo or '',j.name))
            end
        end
    else
        http.send('ERR\r\n')
    end
end

ui_actions=
{
    ['main']            = { 'xupnpd', ui_main },
    ['error']           = { 'xupnpd - error', ui_error },
    ['downloads']       = { 'xupnpd - downloads', ui_downloads },
    ['playlists']       = { 'xupnpd - playlists', ui_playlists },
    ['feeds']           = { 'xupnpd - feeds', ui_feeds },
    ['show']            = { 'xupnpd - show', ui_show },
    ['remove']          = { 'xupnpd - remove', ui_remove },
    ['remove_feed']     = { 'xupnpd - remove feed', ui_remove_feed },
    ['reload']          = { 'xupnpd - reload', ui_reload },
    ['reload_feeds']    = { 'xupnpd - reload feeds', ui_reload_feeds },
    ['save_feeds']      = { 'xupnpd - save feeds', ui_save_feeds },
    ['add_feed']        = { 'xupnpd - add feed', ui_add_feed },
    ['config']          = { 'xupnpd - config', ui_config },
    ['status']          = { 'xupnpd - status', ui_status },
    ['kill']            = { 'xupnpd - kill', ui_kill },
    ['upload']          = { 'xupnpd - upload', ui_upload },
    ['apply']           = { 'xupnpd - apply', ui_apply },
    ['vk_landing']      = { 'xupnpd - vkontakte sign-in redirect', ui_vk_landing },
    ['vk_update']       = { 'xupnpd - vkontakte sign-in result', ui_vk_update },
    ['vk_status']       = { 'xupnpd - vkontakte status', ui_vk_status },
    ['fhelp']           = { 'xupnpd - feeds help', ui_fhelp }
}

function ui_handler(args,data,ip,url)
    local action=string.match(url,'^/ui/(.+)$')

    if action=='style' then
        http_send_headers(200,'css')
        http.sendfile(cfg.ui_path..'bootstrap.min.css')
        return
    elseif action=='api' then
        ui_api_call(args)
        return
    end

    if action and string.find(action,'.+%.m3u$') then
        ui_download(action)
        return
    end

    if not action then action='main' end

    http_send_headers(200,'html')

    local act=ui_actions[action]

    if not act then act=ui_actions['error'] end

    http_vars.title=act[1]
    http_vars.content=act[2]

    ui_args=args
    ui_data=data

    http.sendtfile(cfg.ui_path..'ui_template.html',http_vars)
end
