local to = ".build/"..(_ACTION or "nullaction")
local toolchain = _OPTIONS["vsver"] or "vs2022"

if _ACTION == "gmake2" then
    error("Use `premake5 gmake` instead; gmake2 neglects to link resources.")
end


--------------------------------------------------------------------------------
local function init_configuration(cfg)
    filter {cfg}
        defines("BUILD_"..cfg:upper())
        targetdir(to.."/bin/%{cfg.buildcfg}/%{cfg.platform}")
        objdir(to.."/obj/")
end

--------------------------------------------------------------------------------
local function define_exe(name, exekind)
    project(name)
    flags("fatalwarnings")
    language("c++")
    kind(exekind or "consoleapp")
end


--------------------------------------------------------------------------------
workspace("dirx")
    configurations({"debug", "release"})
    platforms({"x64"})
    location(to)

    characterset("Unicode")
    flags("NoManifest")
    staticruntime("on")
    symbols("on")
    exceptionhandling("on")

    init_configuration("release")
    init_configuration("debug")

    filter "action:vs*"
        pchheader("pch.h")
        pchsource("pch.cpp")

    filter "debug"
        rtti("on")
        optimize("off")
        defines("DEBUG")
        defines("_DEBUG")

    filter "release"
        rtti("off")
        optimize("full")
        omitframepointer("on")
        defines("NDEBUG")

    filter {"release", "action:vs*"}
        flags("LinkTimeOptimization")

--------------------------------------------------------------------------------
define_exe("dirx")
    targetname("dirx")
    links("kernel32")
    links("user32")
    links("advapi32")
    links("shell32")

    includedirs(".build/" .. toolchain .. "/bin") -- for the generated manifest.xml
    files("*.cpp")
    files("wildmatch/*.cpp")
    files("main.rc")

    filter "action:vs*"
        defines("_CRT_SECURE_NO_WARNINGS")
        defines("_CRT_NONSTDC_NO_WARNINGS")



--------------------------------------------------------------------------------
local any_warnings_or_failures = nil
local msbuild_locations = {
    "c:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin",
    "c:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\MSBuild\\Current\\Bin",
    "c:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise\\MSBuild\\Current\\Bin",
}

--------------------------------------------------------------------------------
local release_manifest = {
    "dirx.exe",
}

--------------------------------------------------------------------------------
-- Some timestamp services for code signing:
--  http://timestamp.digicert.com
--  http://time.certum.pl
--  http://sha256timestamp.ws.symantec.com/sha256/timestamp
--  http://timestamp.comodoca.com/authenticode
--  http://timestamp.comodoca.com
--  http://timestamp.sectigo.com
--  http://timestamp.globalsign.com
--  http://tsa.starfieldtech.com
--  http://timestamp.entrust.net/TSS/RFC3161sha2TS
--  http://tsa.swisssign.net
local cert_name = "Open Source Developer, Christopher Antos"
--local timestamp_service = "http://timestamp.digicert.com"
local timestamp_service = "http://time.certum.pl"
local sign_command = string.format(' sign /n "%s" /fd sha256 /td sha256 /tr %s ', cert_name, timestamp_service)
local verify_command = string.format(' verify /pa ')

--------------------------------------------------------------------------------
local function warn(msg)
    print("\x1b[0;33;1mWARNING: " .. msg.."\x1b[m")
    any_warnings_or_failures = true
end

--------------------------------------------------------------------------------
local function failed(msg)
    print("\x1b[0;31;1mFAILED: " .. msg.."\x1b[m")
    any_warnings_or_failures = true
end

--------------------------------------------------------------------------------
local exec_lead = "\n"
local function exec(cmd, silent)
    print(exec_lead .. "## EXEC: " .. cmd)

    if silent then
        cmd = "1>nul 2>nul "..cmd
    else
        -- cmd = "1>nul "..cmd
    end

    -- Premake replaces os.execute() with a version that runs path.normalize()
    -- which converts \ to /.  That can cause problems when executing some
    -- programs such as cmd.exe.
    local prev_norm = path.normalize
    path.normalize = function (x) return x end
    local _, _, ret = os.execute(cmd)
    path.normalize = prev_norm

    return ret == 0
end

--------------------------------------------------------------------------------
local function exec_with_retry(cmd, tries, delay, silent)
    while tries > 0 do
        if exec(cmd, silent) then
            return true
        end

        tries = tries - 1

        if tries > 0 then
            print("... waiting to retry ...")
            local target = os.clock() + delay
            while os.clock() < target do
                -- Busy wait, but this is such a rare case that it's not worth
                -- trying to be more efficient.
            end
        end
    end

    return false
end

--------------------------------------------------------------------------------
local function mkdir(dir)
    if os.isdir(dir) then
        return
    end

    local ret = exec("md " .. path.translate(dir), true)
    if not ret then
        error("Failed to create directory '" .. dir .. "' ("..tostring(ret)..")", 2)
    end
end

--------------------------------------------------------------------------------
local function rmdir(dir)
    if not os.isdir(dir) then
        return
    end

    return exec("rd /q /s " .. path.translate(dir), true)
end

--------------------------------------------------------------------------------
local function unlink(file)
    return exec("del /q " .. path.translate(file), true)
end

--------------------------------------------------------------------------------
local function copy(src, dest)
    src = path.translate(src)
    dest = path.translate(dest)
    return exec("copy /y " .. src .. " " .. dest, true)
end

--------------------------------------------------------------------------------
local function rename(src, dest)
    src = path.translate(src)
    return exec("ren " .. src .. " " .. dest, true)
end

--------------------------------------------------------------------------------
local function file_exists(name)
    local f = io.open(name, "r")
    if f ~= nil then
        io.close(f)
        return true
    end
    return false
end

--------------------------------------------------------------------------------
local function have_required_tool(name, fallback)
    local vsver
    if name == "msbuild" then
        local opt_vsver = _OPTIONS["vsver"]
        if opt_vsver and opt_vsver:find("^vs") then
            vsver = opt_vsver:sub(3)
        end
    end

    if not vsver then
        if exec("where " .. name, true) then
            return name
        end
    end

    if fallback then
        local t
        if type(fallback) == "table" then
            t = fallback
        else
            t = { fallback }
        end
        for _,dir in ipairs(t) do
            if not vsver or dir:find(vsver) then
                local file = dir.."\\"..name..".exe"
                if file_exists(file) then
                    return '"'..file..'"'
                end
            end
        end
    end

    return nil
end

local function parse_version_file()
    local ver_file = io.open("version.h")
    if not ver_file then
        error("Failed to open version.h file")
    end
    local vmaj, vmin
    for line in ver_file:lines() do
        if not vmaj then
            vmaj = line:match("VERSION_MAJOR%s+([^%s]+)")
        end
        if not vmin then
            vmin = line:match("VERSION_MINOR%s+([^%s]+)")
        end
    end
    ver_file:close()
    if not (vmaj and vmin) then
        error("Failed to get version info")
    end
    return vmaj .. "." .. vmin
end

--------------------------------------------------------------------------------
newaction {
    trigger = "release",
    description = "Dirx: Creates a release of Dirx",
    execute = function ()
        local premake = _PREMAKE_COMMAND
        local root_dir = path.getabsolute(".build/release") .. "/"

        -- Check we have the tools we need.
        local have_msbuild = have_required_tool("msbuild", msbuild_locations)
        local have_7z = have_required_tool("7z", { "c:\\Program Files\\7-Zip", "c:\\Program Files (x86)\\7-Zip" })
        local have_signtool = have_required_tool("signtool", { "c:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22000.0\\x64" })

        -- Clone repo in release folder and checkout the specified version
        local code_dir = root_dir .. "~working/"
        rmdir(code_dir)
        mkdir(code_dir)

        exec("git clone . " .. code_dir)
        if not os.chdir(code_dir) then
            error("Failed to chdir to '" .. code_dir .. "'")
        end
        exec("git checkout " .. (_OPTIONS["commit"] or "HEAD"))

        -- Build the code.
        local x64_ok = true
        local toolchain = "ERROR"
        local build_code = function (target)
            if have_msbuild then
                target = target or "build"

                toolchain = _OPTIONS["vsver"] or "vs2022"
                exec(premake .. " " .. toolchain)
                os.chdir(".build/" .. toolchain)

                x64_ok = exec(have_msbuild .. " /m /v:q /p:configuration=release /p:platform=x64 dirx.sln /t:" .. target)

                os.chdir("../..")
            else
                error("Unable to locate msbuild.exe")
            end
        end

        -- Build everything.
        exec(premake .. " manifest")
        build_code()

        local src = path.getabsolute(".build/" .. toolchain .. "/bin/release").."/"

        -- Do a coarse check to make sure there's a build available.
        if not os.isdir(src .. ".") or not x64_ok then
            error("There's no build available in '" .. src .. "'")
        end

        -- Now we can sign the files.
        local sign = not _OPTIONS["nosign"]
        local signed_ok -- nil means unknown, false means failed, true means ok.
        local function sign_files(file_table)
            local orig_dir = os.getcwd()
            os.chdir(src)

            local files = ""
            for _, file in ipairs(file_table) do
                files = files .. " " .. file
            end

            -- Sectigo requests to wait 15 seconds between timestamps.
            -- Digicert and Certum don't mention any delay, so for now
            -- just let signtool do the signatures and timestamps without
            -- imposing external delays.
            signed_ok = (exec('"' .. have_signtool .. sign_command .. files .. '"') and signed_ok ~= false) or false
            -- Note: FAILS: cmd.exe /c "program" args "more args"
            --    SUCCEEDS: cmd.exe /c ""program" args "more args""

            -- Verify the signatures.
            signed_ok = (exec(have_signtool .. verify_command .. files) and signed_ok ~= false) or false

            os.chdir(orig_dir)
        end

        if sign then
            sign_files({"x64\\dirx.exe"})
        end

        -- Parse version.
        local version = parse_version_file()

        -- Now we know the version we can create our output directory.
        local target_dir = root_dir .. os.date("%Y%m%d_%H%M%S") .. "_" .. version .. "/"
        rmdir(target_dir)
        mkdir(target_dir)

        -- Copy release files.
        copy(src .. "/x64/dirx.exe", root_dir)
        copy(src .. "/x64/dirx.pdb", root_dir)

        -- Package the release and the pdbs separately.
        os.chdir(src .. "/x64")
        if have_7z then
            exec(have_7z .. " a -r  " .. target_dir .. "dirx-v" .. version .. "-symbols.zip  *.pdb")
            exec(have_7z .. " a -r  " .. target_dir .. "dirx-v" .. version .. ".zip  *.exe")
        end

        -- Tidy up code directory.
        os.chdir(root_dir)
        rmdir(code_dir)

        -- Report some facts about what just happened.
        print("\n\n")
        if not have_7z then     warn("7-ZIP NOT FOUND -- Packing to .zip files was skipped.") end
        if not x64_ok then      failed("x64 BUILD FAILED") end
        if sign and not signed_ok then
            failed("signing FAILED")
        end
        if not any_warnings_or_failures then
            print("\x1b[0;32;1mRelease " .. version .. " built successfully.\x1b[m")
        end
    end
}

--------------------------------------------------------------------------------
newoption {
    trigger     = "nosign",
    description = "Dirx: don't sign the release files"
}

--------------------------------------------------------------------------------
newaction {
    trigger = "manifest",
    description = "Dirx: generate app manifest",
    execute = function ()
        toolchain = _OPTIONS["vsver"] or "vs2022"
        local outdir = path.getabsolute(".build/" .. toolchain .. "/bin").."/"

        local version = parse_version_file()

        local src = io.open("manifest_src.xml")
        if not src then
            error("Failed to open manifest_src.xml input file")
        end
        local dst = io.open(outdir .. "manifest.xml", "w")
        if not dst then
            error("Failed to open manifest.xml output file")
        end

        for line in src:lines("*L") do
            line = line:gsub("%%VERSION%%", version)
            dst:write(line)
        end

        src:close()
        dst:close()

        print("Generated manifest.xml in " .. outdir:gsub("/", "\\") .. ".")
    end
}

