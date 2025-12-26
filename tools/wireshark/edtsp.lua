-- EDTSP Wireshark Dissector
-- Protocol: EDTSP (ED61 Transport Protocol)
-- Author: Auto-generated for EDTSP project

-- Create protocol object
local edtsp_proto = Proto("EDTSP", "ED61 Transport Protocol")

-- Protocol fields
local f_magic = ProtoField.uint16("edtsp.magic", "Magic", base.HEX)
local f_type = ProtoField.uint8("edtsp.type", "Packet Type", base.DEC)
local f_source_id = ProtoField.uint32("edtsp.source_id", "Source ID", base.HEX)
local f_payload_len = ProtoField.uint8("edtsp.payload_len", "Payload Length", base.DEC)

-- Payload fields (type-specific)
local f_iface_type = ProtoField.uint8("edtsp.iface_type", "Interface Type", base.DEC)
local f_version = ProtoField.uint8("edtsp.version", "Protocol Version", base.DEC)
local f_device_name = ProtoField.string("edtsp.device_name", "Device Name")
local f_role = ProtoField.uint8("edtsp.role", "Role", base.DEC)
local f_uptime = ProtoField.uint32("edtsp.uptime_ms", "Uptime (ms)", base.DEC)
local f_active_devices = ProtoField.uint8("edtsp.active_devices", "Active Devices", base.DEC)
local f_handshake_step = ProtoField.uint8("edtsp.handshake_step", "Handshake Step", base.DEC)
local f_target_id = ProtoField.uint32("edtsp.target_id", "Target ID", base.HEX)
local f_capabilities = ProtoField.uint16("edtsp.capabilities", "Capabilities", base.HEX)
local f_sensor_id = ProtoField.uint8("edtsp.sensor_id", "Sensor ID", base.DEC)
local f_sampling_rate = ProtoField.uint16("edtsp.sampling_rate_ms", "Sampling Rate (ms)", base.DEC)
local f_enable = ProtoField.uint8("edtsp.enable", "Enable", base.DEC)
local f_timestamp = ProtoField.uint32("edtsp.timestamp_ms", "Timestamp (ms)", base.DEC)
local f_data_len = ProtoField.uint8("edtsp.data_len", "Data Length", base.DEC)
local f_data = ProtoField.bytes("edtsp.data", "Sensor Data")

-- Register fields
edtsp_proto.fields = {
    f_magic, f_type, f_source_id, f_payload_len,
    f_iface_type, f_version, f_device_name,
    f_role, f_uptime, f_active_devices,
    f_handshake_step, f_target_id, f_capabilities,
    f_sensor_id, f_sampling_rate, f_enable,
    f_timestamp, f_data_len, f_data
}

-- Packet type names
local packet_types = {
    [1] = "DISCOVERY",
    [2] = "HEARTBEAT",
    [3] = "HANDSHAKE",
    [4] = "CONFIG",
    [5] = "DATA"
}

-- Role names
local role_names = {
    [0] = "UNKNOWN",
    [1] = "SLAVE",
    [2] = "MASTER"
}

-- Interface names
local iface_names = {
    [0] = "UNKNOWN",
    [1] = "ETHERNET",
    [2] = "WIFI",
    [3] = "5G"
}

-- Dissector function
function edtsp_proto.dissector(buffer, pinfo, tree)
    -- Check minimum packet size
    if buffer:len() < 8 then
        return 0
    end
    
    -- Check magic number
    local magic = buffer(0, 2):uint()
    if magic ~= 0xED61 then
        return 0
    end
    
    -- Set protocol column
    pinfo.cols.protocol = "EDTSP"
    
    -- Parse header
    local pkt_type = buffer(2, 1):uint()
    local source_id = buffer(3, 4):uint()
    local payload_len = buffer(7, 1):uint()
    
    local type_name = packet_types[pkt_type] or "UNKNOWN"
    
    -- Set info column
    pinfo.cols.info = string.format("%s from 0x%08X", type_name, source_id)
    
    -- Create protocol tree
    local subtree = tree:add(edtsp_proto, buffer(), "EDTSP Protocol")
    
    -- Add header fields
    local header_tree = subtree:add(buffer(0, 8), "Header")
    header_tree:add(f_magic, buffer(0, 2))
    header_tree:add(f_type, buffer(2, 1)):append_text(" (" .. type_name .. ")")
    header_tree:add(f_source_id, buffer(3, 4))
    header_tree:add(f_payload_len, buffer(7, 1))
    
    -- Parse payload based on type
    local offset = 8
    
    if pkt_type == 1 then  -- DISCOVERY
        if buffer:len() >= offset + 2 + 32 then
            local payload_tree = subtree:add(buffer(offset), "Discovery Payload")
            local iface = buffer(offset, 1):uint()
            payload_tree:add(f_iface_type, buffer(offset, 1)):append_text(" (" .. (iface_names[iface] or "UNKNOWN") .. ")")
            payload_tree:add(f_version, buffer(offset + 1, 1))
            payload_tree:add(f_device_name, buffer(offset + 2, 32))
        end
        
    elseif pkt_type == 2 then  -- HEARTBEAT
        if buffer:len() >= offset + 6 then
            local payload_tree = subtree:add(buffer(offset), "Heartbeat Payload")
            local role = buffer(offset, 1):uint()
            payload_tree:add(f_role, buffer(offset, 1)):append_text(" (" .. (role_names[role] or "UNKNOWN") .. ")")
            payload_tree:add(f_uptime, buffer(offset + 1, 4))
            payload_tree:add(f_active_devices, buffer(offset + 5, 1))
            
            -- Update info with role
            pinfo.cols.info = pinfo.cols.info .. " [" .. (role_names[role] or "UNKNOWN") .. "]"
        end
        
    elseif pkt_type == 3 then  -- HANDSHAKE
        if buffer:len() >= offset + 8 then
            local payload_tree = subtree:add(buffer(offset), "Handshake Payload")
            payload_tree:add(f_handshake_step, buffer(offset, 1))
            payload_tree:add(f_target_id, buffer(offset + 1, 4))
            payload_tree:add(f_capabilities, buffer(offset + 5, 2))
            local iface = buffer(offset + 7, 1):uint()
            payload_tree:add(f_iface_type, buffer(offset + 7, 1)):append_text(" (" .. (iface_names[iface] or "UNKNOWN") .. ")")
        end
        
    elseif pkt_type == 4 then  -- CONFIG
        if buffer:len() >= offset + 8 then
            local payload_tree = subtree:add(buffer(offset), "Config Payload")
            payload_tree:add(f_target_id, buffer(offset, 4))
            payload_tree:add(f_sensor_id, buffer(offset + 4, 1))
            payload_tree:add(f_sampling_rate, buffer(offset + 5, 2))
            payload_tree:add(f_enable, buffer(offset + 7, 1))
        end
        
    elseif pkt_type == 5 then  -- DATA
        if buffer:len() >= offset + 6 then
            local payload_tree = subtree:add(buffer(offset), "Data Payload")
            payload_tree:add(f_sensor_id, buffer(offset, 1))
            payload_tree:add(f_timestamp, buffer(offset + 1, 4))
            local data_len = buffer(offset + 5, 1):uint()
            payload_tree:add(f_data_len, buffer(offset + 5, 1))
            if buffer:len() >= offset + 6 + data_len then
                payload_tree:add(f_data, buffer(offset + 6, data_len))
            end
        end
    end
    
    return buffer:len()
end

-- Register dissector
-- Try heuristic detection first (magic number based)
function heuristic_checker(buffer, pinfo, tree)
    if buffer:len() < 8 then
        return false
    end
    
    local magic = buffer(0, 2):uint()
    if magic == 0xED61 then
        edtsp_proto.dissector(buffer, pinfo, tree)
        return true
    end
    
    return false
end

-- Register as heuristic dissector
edtsp_proto:register_heuristic("udp", heuristic_checker)

-- Also register for specific UDP port
local udp_table = DissectorTable.get("udp.port")
udp_table:add(5000, edtsp_proto)

print("EDTSP dissector loaded successfully!")
