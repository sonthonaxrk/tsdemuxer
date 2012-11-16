profiles['LG']=
{
    ['desc']='LG TV',

    -- Linux/2.6.31-1.0 UPnP/1.0 DLNADOC/1.50 INTEL_NMPR/2.0 LGE_DLNA_SDK/1.5.0
    -- Mozilla/5.0 (compatible; MSIE 8.0; Windows NT 5.1; LG_UA; AD_LOGON=LGE.NET; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30; .NET CLR 3.0.04506.648; LG_UA; AD_LOGON=LGE.NET; LG NetCast.TV-2010)
    ['match']=function(user_agent)
                if string.find(user_agent,'LGE_DLNA_SDK',1,true) or string.find(user_agent,'LG NetCast',1,true)
                    then return true
                else
                    return false
                end
            end,

    ['options']={},

    ['mime_types']={}
}
