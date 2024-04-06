-- RTP hdrext dissector for Wireshark.
-- To enable, put it into your wireshark lua plugin directory which can be
-- found by navigating Help->About Wireshark->Folders
-- Useful references:
-- https://wiki.wireshark.org/Lua/Examples
-- https://wiki.wireshark.org/uploads/__moin_import__/attachments/Lua/Examples/dissector.lua (thanks Hadriel!)

local twcc = Proto("rtp.ext.rfc5285.twcc", "Transport-wide sequence number")
local twcc_seqnum = ProtoField.uint16("seq_num", "Transport-wide sequence number", base.DEC)
twcc.fields = {
    twcc_seqnum
}
function twcc.dissector(buffer, pinfo, root)
    if buffer:reported_length_remaining() ~= 2 then return end
    root:add(twcc_seqnum, buffer:range(0, 2))
end

local mid = Proto("rtp.ext.rfc5285.mid", "RTP MID extension")
local mid_value = ProtoField.string("mid", "RTP MID extension", base.ASCII)
mid.fields = {
    mid_value
}
function mid.dissector(buffer, pinfo, root)
    -- if buffer:reported_length_remaining() ~= 2 then return end
    root:add(mid_value, buffer:range(0, buffer:reported_length_remaining()))
end

-- preferences. TODO: can these be merged into one? 
-- settings, configurable in the UI.
local settings = {
    twcc = 3,
    mid = 4,
}

local hdr_exts = DissectorTable.get("rtp.ext.rfc5285.id")
hdr_exts:add(settings.twcc, twcc);
hdr_exts:add(settings.mid, mid);


function twcc.prefs_changed()
   local ext = DissectorTable.get("rtp.ext.rfc5285.id")
   ext:remove(settings.twcc, twcc);
   settings.twcc = twcc.prefs.twcc
   ext:add(settings.twcc, twcc);
end

function mid.prefs_changed()
    local ext = DissectorTable.get("rtp.ext.rfc5285.id")
    ext:add(settings.twcc, twcc);
    ext:remove(settings.mid, mid);
    settings.mid = mid.prefs.mid;
    ext:add(settings.mid, mid);
end

twcc.prefs.twcc = Pref.range("TWCC RTP header extension", settings.twcc,
    "RTP header extension id which will be interpreted as twcc sequence number " ..
    "Values must be in the range 1-16", 16)
mid.prefs.mid = Pref.range("MID RTP header extension", settings.mid,
    "RTP header extension id which will be interpreted as mid " ..
    "Values must be in the range 1-16", 16)


