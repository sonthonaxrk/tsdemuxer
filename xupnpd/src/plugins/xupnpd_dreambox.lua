-- Plugin for converting channels lists from DVB receivers based on Enigma2
-- Copyright (C) 2011 Igor Drach
-- leaigor@gmail.com

cfg.dreambox_enigmapath="enigma2/"
function dreambox_updatefeed(feed,friendly_name)
	local feedspath=cfg.feeds_path
	local dreambox_url=feed
	local filein=io.open(cfg.dreambox_enigmapath.."lamedb")

-- Пропускаем ненужные строки
	local instring
	repeat
    		instring=filein:read()
	until instring=="services"

-- Получаем полный список каналов
	local channels={}
	local chanparams
	repeat
	    local chanparam={}
	    chanparams=string.lower(filein:read())
	    if chanparams~="end" then
	        local channame=filein:read()
		filein:read()
		local param=1
		local word
		for word in string.gmatch(chanparams,"[0-9a-f]+") do
		    chanparam[param]=tonumber(word,16)
		    param=param+1
		end
		channels[string.format("%x%x%x%x",chanparam[1],chanparam[2],chanparam[3],chanparam[4])]=channame
	    end
	until chanparams=="end"
	filein:close()

-- Создаем группы каналов
	local userbouquets=io.popen("ls "..cfg.dreambox_enigmapath.."userbouquet.*.tv | xargs -n1 basename")
	for userbouquet in userbouquets:lines() do
	    filein=io.open(cfg.dreambox_enigmapath..userbouquet)
	    local fileout=io.open(feedspath..userbouquet..".m3u","w")
	    local groupname=filein:read()
	    groupname=string.gsub(groupname,"#NAME ","")
	    groupname=string.gsub(groupname,"\r","")
	    fileout:write("#EXTM3U name=\""..groupname.."\"\n")
--    print(groupname)
	    for chanparams in filein:lines() do
	        chanparams=string.lower(string.gsub(chanparams,"#SERVICE ",""))
	        chanparams=string.gsub(chanparams,"\r","")
		local param=1
		local word
	        local chanparam={}
		for word in string.gmatch(chanparams,"[0-9a-f]+") do
		    chanparam[param]=tonumber(word,16)
		    param=param+1
		end
		local channame=channels[string.format("%x%x%x%x",chanparam[4],chanparam[7],chanparam[5],chanparam[6])]
		fileout:write("#EXTINF:0,"..channame.."\n")
		fileout:write(dreambox_url..chanparams.."\n")
	    end
	    fileout:close()
	    filein:close()
	end
end

plugins['dreambox']={}
plugins.dreambox.name="DreamBox"
plugins.dreambox.desc=" url (example: <i>http://192.168.0.1:8001/</i>)"
plugins.dreambox.updatefeed=dreambox_updatefeed
