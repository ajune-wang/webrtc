-- AV1 dissector for Wireshark.
-- To enable, put it into your wireshark lua plugin directory which can be
-- found by navigating Help->About Wireshark->Folders
-- Useful references:
-- https://wiki.wireshark.org/Lua/Examples
-- https://wiki.wireshark.org/uploads/__moin_import__/attachments/Lua/Examples/dissector.lua (thanks Hadriel!)

-- AV1 RTP reference:
-- https://aomediacodec.github.io/av1-rtp-spec/#4-payload-format

function read_leb128(buffer)
    local value = 0;
    local len = 0
    for i=0,8 do
        value = value + buffer:range(len, 1):bitfield(1, 7) * 2 ^ (i * 7)
        len = len + 1
        if buffer:range(len, 1):bitfield(0, 1) == 0 then
            break
        end
    end
    return value, len
end

av1 = Proto("av1", "AV1 Video Codec")
aggr_hdr = ProtoField.uint8("av1.aggr", "AV1 Aggregation header", base.HEX)
aggr_hdr_z = ProtoField.new("Aggregration header Z bit", "av1.aggr.z", ftypes.BOOLEAN,
    {"Continuation from previous packet", "Not a continuation from previous packet"}, 8, 0x80,
    "True if the first is a continuation from the previous packet")
aggr_hdr_y = ProtoField.new("Aggregration header Y bit", "av1.aggr.y", ftypes.BOOLEAN,
    {"Last OBU continues in next packet", "Last OBU ends in this packet"}, 8, 0x40,
    "True if the last OBU continues in the next packet")
aggr_hdr_w = ProtoField.new("Aggregration header W field", "av1.aggr.w", ftypes.UINT8,
    {
        [0] = "All OBUs are length-prefixed",
        [1] = "One OBU, not length prefixed",
        [2] = "All OBUs but the last are length prefixed"
    },
    base.DEC, 0x30,
    "Number of OBUs contained. If W is zero all OBUs are length prefixed, otherwise all but the last one")
aggr_hdr_n = ProtoField.new("Aggregration header N bit", "av1.aggr.n", ftypes.BOOLEAN, nil, 8, 0x08,
    "True if this is the first packet of a codec video sequence")
aggr_hdr_f = ProtoField.new("Aggregration header F bits", "av1.aggr.f", ftypes.BOOLEAN, nil, 8, 0x07,
    "Forbidden bits")
av1.fields = {
    aggr_hdr, aggr_hdr_z, aggr_hdr_y, aggr_hdr_w, aggr_hdr_n, aggr_hdr_f
}

obu = Proto("av1.obu", "AV1 OBU")
obu_f = ProtoField.new("AV1 OBU F bit", "av1.obu.f", ftypes.BOOLEAN, nil, 8, 0x80,
    "Forbidden bit")
obu_type = ProtoField.new("AV1 OBU type", "av1.obu.type", ftypes.UINT8,
    {
        [1] = "Sequence Header",
        [2] = "Temporal Delimiter",
        [3] = "Frame Header",
        [4] = "Tile Group",
        [5] = "Metadata",
        [6] = "Frame",
        [7] = "Redudant Frame Header",
        [8] = "Tile list",
        [15] = "Padding",
    }, base.HEX, 0x78, "TODO")
obu_x = ProtoField.new("AV1 OBU extension bit", "av1.obu.x", ftypes.BOOLEAN, nil, 8, 0x4,
    "True if the OBU has extension (TODO: parsing is missing)")
obu_size = ProtoField.new("AV1 OBU size bit", "av1.obu.size", ftypes.BOOLEAN, {"Size set", "Size not set"}, 8, 0x2,
    "True if the OBU specifies a size. SHOULD be set to zero in RTP")
obu.fields = {
    obu_f, obu_type, obu_x, obu_size
}

obu_frag  = Proto("av1.obu.frag", "AV1 OBU fragment")

function show_aggregation_header(buffer, tree)
    tree:add(aggr_hdr_z, buffer:range(0, 1))
    tree:add(aggr_hdr_y, buffer:range(0, 1))
    tree:add(aggr_hdr_w, buffer:range(0, 1))
    tree:add(aggr_hdr_n, buffer:range(0, 1))
    tree:add(aggr_hdr_f, buffer:range(0, 1))
end

function show_obu_header(buffer, offset, tree)
    tree:add(obu_f, buffer:range(offset, 1))
    tree:add(obu_type, buffer:range(offset, 1))
    tree:add(obu_x, buffer:range(offset, 1))
    tree:add(obu_size, buffer:range(offset, 1))
end

function av1.dissector(buffer, pinfo, root)
    -- buffer is the RTP payload at this point.
    -- TODO: look at buffer:reported_length_remaining()
    pinfo.cols.protocol:set(av1.name)
    local tree = root:add(av1, buffer())
    show_aggregation_header(buffer, tree)

    -- The W field determines whether OBUs are length prefixed
    local w = buffer:range(0, 1):bitfield(2, 2)
    -- The Z flag determines whether this is a continuation of an OBU from a previous packet
    local z = buffer:range(0, 1):bitfield(0, 1)
    local obu_tree
    local offset = 1
    if w == 0 then
        -- all OBUs including the last are length-prefixed
        while offset <= buffer:reported_length_remaining() do
            local len, consumed = read_leb128(buffer:range(offset))
            offset = offset + consumed + len
            show_obu_header(buffer, offset, tree:add(obu, buffer:range(offset, consumed + len)))
        end
    elseif w == 1 then
        -- single OBU, not length prefixed as it is the last one
        if (z == 0) then
            show_obu_header(buffer, offset, tree:add(obu, buffer:range(offset)))
        else
            tree:add(obu_frag, buffer:range(offset))
        end
    elseif w == 2 then
        -- first OBU is length-prefixed
        local len, consumed = read_leb128(buffer:range(offset))
        show_obu_header(buffer, offset, tree:add(obu, buffer:range(offset, consumed + len)))

        offset = offset + consumed + len
        show_obu_header(buffer, offset, tree:add(obu, buffer:range(offset)))
    end
end

-- settings, configurable in the UI.
local settings = {
    payload_type = 35,
}

function av1.prefs_changed()
  local rtp = DissectorTable.get("rtp.pt")
  rtp:remove(settings.payload_type, av1);
  settings.payload_type = av1.prefs.pt
  rtp:add(settings.payload_type, av1);
end

av1.prefs.pt = Pref.range("AV1 dynamic payload types", settings.payload_type,
    "dynamic payload types which will be interpreted as av1; " ..
    "Values must be in the range 1-127", 128)

