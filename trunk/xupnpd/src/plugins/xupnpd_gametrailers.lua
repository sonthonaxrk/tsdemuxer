-- Copyright (C) 2011 I. Sokolov
-- happy.neko@gmail.com
-- Licensed under GNU GPL version 2 
-- https://www.gnu.org/licenses/gpl-2.0.html


gametrailers_feeds=
{
    ['trailers']        = 'http://www.gametrailers.com/videos-trailers/feed',
    ['reviews']         = 'http://www.gametrailers.com/reviews/feed',
    ['shows']           = 'http://www.gametrailers.com/shows/feed',
    ['xbox']            = 'http://www.gametrailers.com/xbox-360/feed',
    ['ps3']             = 'http://www.gametrailers.com/ps3/feed',
    ['wiiu']            = 'http://www.gametrailers.com/wii-u/feed',
    ['pc']              = 'http://www.gametrailers.com/pc/feed',
    ['vita']            = 'http://www.gametrailers.com/vita/feed',
    ['3ds']             = 'http://www.gametrailers.com/3ds/feed'
}

gametrailers_download_url='http://gametrailers.com/download'
gametrailers_images_url='http://gametrailers.mtvnimages.com/images'

function rss_find_child(x,name)
    if x.elements then
        for i,j in ipairs(x.elements) do
            if j.name==name then return j end
        end
    end
    return nil
end

function rss_parse_feed(feed_data)
    local t={}

    local x=xml.decode(feed_data)

    if x and x.name=='rss' then
        x=rss_find_child(x,'channel')

        if x and x.elements then
            local idx=1
            for i,j in ipairs(x.elements) do
                if j.name=='item' then
                    local title=rss_find_child(j,'title') if title then title=title.value end
                    local link=rss_find_child(j,'link') if link then link=link.value end
                    if title and link then
                        t[idx]={ ['title']=title, ['link']=link }
                        idx=idx+1
                    end
                end
            end
        end
    end

    return t
end

function gametrailers_updatefeed(feed,friendly_name)
    local rc=false

    local feed_name='gametrailers_'..string.gsub(feed,'/','_')
    local feed_m3u_path=cfg.feeds_path..feed_name..'.m3u'
    local tmp_m3u_path=cfg.tmp_path..feed_name..'.m3u'

    local feed_url=gametrailers_feeds[feed]

    if not feed_url then return false end

    if cfg.debug>0 then print('Game Trailers try url '..feed_url) end

    local feed_data=http.download(feed_url)

    if feed_data then
        local x=rss_parse_feed(feed_data)

        feed_data=nil

        if x then
            local dfd=io.open(tmp_m3u_path,'w+')
            if dfd then
                dfd:write('#EXTM3U name=\"',friendly_name or feed_name,'\" type=mp4 plugin=gametrailers\n')

                for i,j in ipairs(x) do
                    dfd:write('#EXTINF:0 ,',j.title,'\n',j.link,'\n')
                end
                dfd:close()

                if util.md5(tmp_m3u_path)~=util.md5(feed_m3u_path) then
                    if os.execute(string.format('mv %s %s',tmp_m3u_path,feed_m3u_path))==0 then
                        if cfg.debug>0 then print('GameTrailers feed \''..feed_name..'\' updated') end
                        rc=true
                    end
                else
                    util.unlink(tmp_m3u_path)
                end
            end
        end
    end

    return rc
end

function gametrailers_sendurl(gametrailers_url,range)

    local url=nil

    if plugin_sendurl_from_cache(gametrailers_url,range) then return end

    local clip_page=http.download(gametrailers_url)

    if clip_page then
        local itemMgid,token=string.match(clip_page,'data%-video="([%w%.:%-]+)"%s*data%-token="(%w+)"')

        clip_page=nil

        if itemMgid and token then
            local u=string.format('http://www.gametrailers.com/feeds/video_download/%s/%s',itemMgid,token)

            clip_page=http.download(u)

            if clip_page then
                local x=json.decode(clip_page)

                clip_page=nil

                if x and x.url then url=x.url end
            end
        end


    end

    if url then
        if cfg.debug>0 then print('GameTrailers clip real URL: '..url) end

        plugin_sendurl(gametrailers_url,url,range)
    else
        if cfg.debug>0 then print('GameTrailers clip real URL is not found') end

        plugin_sendfile('www/corrupted.mp4')
    end
end

plugins['gametrailers']={}
plugins.gametrailers.name="GameTrailers"
plugins.gametrailers.desc="<i>feed</i>"..
"<br/><b>GameTrailers feeds</b>: trailers, reviews, shows, xbox, ps3, wiiu, pc, vita, 3ds"

plugins.gametrailers.sendurl=gametrailers_sendurl
plugins.gametrailers.updatefeed=gametrailers_updatefeed

--gametrailers_updatefeed('ps3')
