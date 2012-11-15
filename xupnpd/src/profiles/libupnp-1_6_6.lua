-- for VLC 1.0.6

profiles['libupnp-1.6.6']=
{
    ['desc']='The portable SDK for UPnP Devices v1.6.6',

    -- Linux/2.6.32-42-generic, UPnP/1.0, Portable SDK for UPnP devices/1.6.6
    ['match']=function(user_agent) if string.find(user_agent,'UPnP devices/1.6.6',1,true) then return true else return false end end,

    ['options']=
    {
        ['soap_length']=false
    },

    ['mime_types']= {}
}
