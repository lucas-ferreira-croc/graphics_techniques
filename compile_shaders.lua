require "lfs"

print("compiling shaders")
function dirtree(dir)
	assert(dir and dir ~= "", "please pass directory parameter")
	if string.sub(dir, -1) == "/" then
		dir = string.sub(dir, 1, -2)
	end

	local function yieldtree(current_dir)
		for entry in lfs.dir(current_dir) do
			if entry ~= "." and entry ~= ".." then
				local fullpath = current_dir .. "/" .. entry
				local attr = lfs.attributes(fullpath)
				coroutine.yield(fullpath, attr)
				if attr and attr.mode == "directory" then
					yieldtree(fullpath)
				end
			end
		end
	end

	return coroutine.wrap(function() yieldtree(dir) end)

end

for filename, attr in dirtree("C:/dev/learning/graphics_techniques/shaders/glsl") do
	if attr and attr.mode == "file" and not filename:match("%.spv$") then
		local spv_path = filename .. ".spv"

		if lfs.attributes(spv_path, "mode") ~= "file" then
			print("compiling shader for: " ..filename)

			local cmd = string.format('glslc.exe "%s" -o "%s"', filename, spv_path)
			local ok, exit_type, code = os.execute(cmd)

			if ok then
				print("generated " .. spv_path)
			else
				print("failed to compile shader for: " .. filename)
			end
		end
	end
end


	
	
	
	
