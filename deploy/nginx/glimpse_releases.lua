-- Glimpse 的版本信息，实时取自 GitLab 项目 kaguya/glimpse（ID 150）：
--
--   GET /kaguya/glimpse/releases.json  版本页用：每个版本的版本号、日期、说明、下载地址
--   GET /kaguya/glimpse/update.json    Glimpse 启动时检查更新用：
--       {"latest": {...}, "required": {...} 或 null}，各带安装包的 SHA-256 和大小
--
-- 只输出这些字段：作者、提交、邮箱等 Release 的其他内容不会公开。
-- 下载地址指向 /glimpse-downloads/<版本>/<文件>（同一个 server 里转发到 GitLab 软件包仓库）。
--
-- 依赖：
--   nginx.conf 的 http{} 里                  lua_shared_dict glimpse 1m;
--   conf.d/pages.yanlei.org.conf 里          两个 location，以及 internal 的 /__glimpse_api/
--                                           （带 GitLab 令牌，令牌在 glimpse-api-token.inc）
--   必须更新的版本                            pages.yanlei.org.conf 里 set $glimpse_required_version
--
-- 仓库里 deploy/nginx/ 有这份文件和 location 片段的副本（不含令牌）。

local cjson = require "cjson.safe"
if cjson.encode_escape_forward_slash then cjson.encode_escape_forward_slash(false) end

local store = ngx.shared.glimpse
local TTL = 60                          -- 结果缓存多久（秒）：发布后最多这么久就能看到
local DOWNLOAD_BASE = "/glimpse-downloads"

local _M = {}

-- GitLab API（项目 150 下的路径），失败时返回 nil 和原因
local function api(path, args)
  local res = ngx.location.capture("/__glimpse_api/" .. path, { args = args })
  if res.status ~= ngx.HTTP_OK then
    return nil, "GitLab " .. path .. " " .. res.status
  end
  local data = cjson.decode(res.body)
  if data == nil then return nil, "GitLab " .. path .. ": not JSON" end
  return data
end

-- 有安装包的版本，新的在前
local function releases()
  local list, err = api("releases", { per_page = 100 })
  if not list then return nil, err end
  local out = setmetatable({}, cjson.array_mt)
  for _, r in ipairs(list) do
    local files = setmetatable({}, cjson.array_mt)
    local links = (type(r.assets) == "table" and r.assets.links) or {}
    for _, link in ipairs(links) do
      local version, name = tostring(link.url or ""):match("/packages/generic/[^/]+/([^/]+)/([^/?]+)")
      if version then
        files[#files + 1] = { name = name, url = DOWNLOAD_BASE .. "/" .. version .. "/" .. name, package_version = version }
      end
    end
    if #files > 0 then
      out[#out + 1] = {
        version = (tostring(r.tag_name or ""):gsub("^v", "")),
        date = tostring(r.released_at or r.created_at or ""):sub(1, 10),
        notes = r.description or "",
        files = files,
      }
    end
  end
  return out
end

-- 软件包仓库里这个文件的记录（同名多次上传时取最新的一次）
local function package_file(version, name)
  local packages, err = api("packages", { package_name = "glimpse", package_version = version, package_type = "generic" })
  if not packages then return nil, err end
  if not packages[1] then return nil, "no package " .. version end
  local files, err2 = api("packages/" .. packages[1].id .. "/package_files", { per_page = 100 })
  if not files then return nil, err2 end
  local found
  for _, f in ipairs(files) do
    if f.file_name == name then found = f end
  end
  if not found or not found.file_sha256 then return nil, "no file " .. name end
  return found
end

local function update_entry(release)
  local file = release.files[1]
  local record, err = package_file(file.package_version, file.name)
  if not record then return nil, err end
  return { version = release.version, url = file.url, sha256 = record.file_sha256, size = record.size, notes = release.notes }
end

local function update()
  local list, err = releases()
  if not list then return nil, err end
  local result = { latest = cjson.null, required = cjson.null }
  if list[1] then
    result.latest = update_entry(list[1]) or cjson.null
  end
  local required = (ngx.var.glimpse_required_version or ""):gsub("^v", "")
  if required ~= "" then
    for _, r in ipairs(list) do
      if r.version == required then
        result.required = update_entry(r) or cjson.null
      end
    end
    if result.required == cjson.null then
      ngx.log(ngx.WARN, "glimpse: required version ", required, " has no release package")
    end
  end
  return result
end

-- 内部字段（package_version）不往外给
local function public_releases()
  local list, err = releases()
  if not list then return nil, err end
  for _, r in ipairs(list) do
    for _, f in ipairs(r.files) do f.package_version = nil end
  end
  return list
end

local function serve(key, build)
  local body = store and store:get(key)
  if not body then
    local data, err = build()
    if not data then
      ngx.log(ngx.ERR, "glimpse ", key, ": ", err)
      ngx.status = ngx.HTTP_BAD_GATEWAY
      ngx.header["Content-Type"] = "application/json; charset=utf-8"
      ngx.header["Cache-Control"] = "no-store"
      ngx.say('{"error":"release information unavailable"}')
      return ngx.exit(ngx.status)
    end
    body = cjson.encode(data)
    if store then store:set(key, body, TTL) end
  end
  ngx.header["Content-Type"] = "application/json; charset=utf-8"
  ngx.header["Cache-Control"] = "public, max-age=" .. TTL
  ngx.header["Access-Control-Allow-Origin"] = "*"
  ngx.say(body)
end

function _M.releases()
  return serve("releases", public_releases)
end

function _M.update()
  -- 必须更新的版本是缓存键的一部分：改了配置马上生效
  return serve("update:" .. (ngx.var.glimpse_required_version or ""), update)
end

return _M
