-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- RIFF chunk parser for semihosting device

local riff = {}

-- Read little-endian u32 from byte table
local function read_u32_le(bytes, offset)
	return bytes[offset] | (bytes[offset + 1] << 8) |
	       (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24)
end

-- Read value with arbitrary byte size and endianness
-- bytes: array of bytes
-- offset: starting position
-- size: number of bytes (1, 2, 4, 8, 16...)
-- endian: 0=LE, 1=BE, 2=PDP
function riff.read_value(bytes, offset, size, endian)
	if size == 0 or size > 8 then
		return 0
	end

	local value = 0

	if endian == 0 then -- Little endian
		for i = 0, size - 1 do
			value = value | (bytes[offset + i] << (i * 8))
		end
	elseif endian == 1 then -- Big endian
		for i = 0, size - 1 do
			value = value | (bytes[offset + i] << ((size - 1 - i) * 8))
		end
	elseif endian == 2 then -- PDP endian (16-bit words, LE word order)
		for i = 0, size - 1, 2 do
			local word_lo = bytes[offset + i]
			local word_hi = bytes[offset + i + 1] or 0
			value = value | (word_lo << ((i + 1) * 8)) | (word_hi << (i * 8))
		end
	end

	return value
end

-- Write value with arbitrary byte size and endianness
function riff.write_value(bytes, offset, value, size, endian)
	if size == 0 or size > 8 then
		return
	end

	if endian == 0 then -- Little endian
		for i = 0, size - 1 do
			bytes[offset + i] = (value >> (i * 8)) & 0xFF
		end
	elseif endian == 1 then -- Big endian
		for i = 0, size - 1 do
			bytes[offset + i] = (value >> ((size - 1 - i) * 8)) & 0xFF
		end
	elseif endian == 2 then -- PDP endian
		for i = 0, size - 1, 2 do
			bytes[offset + i] = (value >> ((i + 1) * 8)) & 0xFF
			bytes[offset + i + 1] = (value >> (i * 8)) & 0xFF
		end
	end
end

-- Parse RIFF header and return form type
-- Returns: form_type string or nil on error
function riff.parse_header(bytes)
	if #bytes < 12 then
		return nil, "Buffer too small for RIFF header"
	end

	-- Check RIFF signature
	if bytes[0] ~= 0x52 or bytes[1] ~= 0x49 or
	   bytes[2] ~= 0x46 or bytes[3] ~= 0x46 then -- 'RIFF'
		return nil, "Invalid RIFF signature"
	end

	local size = read_u32_le(bytes, 4)

	-- Read form type
	local form = string.char(bytes[8], bytes[9], bytes[10], bytes[11])

	if form ~= "SEMI" then
		return nil, "Invalid form type (expected SEMI)"
	end

	return form, size
end

-- Parse all chunks from RIFF data
-- Returns: table of chunks {id, size, offset} or nil on error
function riff.parse_chunks(bytes)
	local form, size = riff.parse_header(bytes)
	if not form then
		return nil, size -- size contains error message
	end

	local chunks = {}
	local offset = 12 -- After RIFF header

	while offset + 8 <= #bytes do
		local chunk = {}

		-- Read chunk ID (4 bytes)
		chunk.id = string.char(bytes[offset], bytes[offset + 1],
		                       bytes[offset + 2], bytes[offset + 3])

		-- Read chunk size (4 bytes, little-endian)
		chunk.size = read_u32_le(bytes, offset + 4)

		-- Chunk data starts after 8-byte header
		chunk.offset = offset + 8

		table.insert(chunks, chunk)

		-- Move to next chunk (pad to even boundary)
		offset = offset + 8 + chunk.size
		if (chunk.size & 1) ~= 0 then
			offset = offset + 1
		end
	end

	return chunks
end

-- Find a specific chunk by ID
function riff.find_chunk(chunks, id)
	for _, chunk in ipairs(chunks) do
		if chunk.id == id then
			return chunk
		end
	end
	return nil
end

-- Parse CNFG chunk
-- Returns: {word_size, ptr_size, endianness} or nil
function riff.parse_cnfg(bytes, chunk)
	if not chunk or chunk.size < 4 then
		return nil, "Invalid CNFG chunk"
	end

	local offset = chunk.offset
	return {
		word_size = bytes[offset],
		ptr_size = bytes[offset + 1],
		endianness = bytes[offset + 2],
		reserved = bytes[offset + 3]
	}
end

-- Parse CALL chunk
-- Returns: {opcode, arg_ptr} or nil
function riff.parse_call(bytes, chunk, ptr_size, endian)
	if not chunk or chunk.size < 4 + ptr_size then
		return nil, "Invalid CALL chunk"
	end

	local offset = chunk.offset
	local opcode = bytes[offset]

	-- arg_ptr starts at offset + 4 (after opcode and 3 reserved bytes)
	local arg_ptr = riff.read_value(bytes, offset + 4, ptr_size, endian)

	return {
		opcode = opcode,
		arg_ptr = arg_ptr
	}
end

-- Create RETN chunk data
-- Returns: table of bytes
function riff.create_retn(result, errno, word_size, endian)
	local bytes = {}
	local size = word_size + 4

	-- Chunk ID: 'RETN'
	bytes[0] = 0x52  -- 'R'
	bytes[1] = 0x45  -- 'E'
	bytes[2] = 0x54  -- 'T'
	bytes[3] = 0x4E  -- 'N'

	-- Chunk size (little-endian u32)
	bytes[4] = size & 0xFF
	bytes[5] = (size >> 8) & 0xFF
	bytes[6] = (size >> 16) & 0xFF
	bytes[7] = (size >> 24) & 0xFF

	-- Result value (word_size bytes, guest endianness)
	riff.write_value(bytes, 8, result, word_size, endian)

	-- Errno (4 bytes, little-endian - always LE per spec)
	bytes[8 + word_size] = errno & 0xFF
	bytes[9 + word_size] = (errno >> 8) & 0xFF
	bytes[10 + word_size] = (errno >> 16) & 0xFF
	bytes[11 + word_size] = (errno >> 24) & 0xFF

	return bytes
end

return riff
