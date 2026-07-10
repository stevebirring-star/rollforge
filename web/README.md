# The RollForge web page

`gen_page.py` generates `index.html` for <https://getstackbase.com/rollforge>.

The **user manual on that page is generated from `src/ui/AboutView.cpp`** — the app's own
in-app Help text — so the page cannot drift from the app. The feature cards and the layout
are hand-written here; everything under "User manual" is not.

## Rebuild and publish

```bash
# 1. Build the packages WITHOUT publishing a GitHub Release.
#    release.yml is workflow_dispatch ONLY -- no tag triggers it (it used to, and every
#    tag then rebuilt both platforms for artifacts nobody downloads). Dispatched against a
#    BRANCH, github.ref_type is 'branch', so the publish job's gate
#        github.ref_type == 'tag' && vars.ROLLFORGE_PUBLISH_RELEASE == 'true'
#    is false and it skips. This matters: the repo is public, so a Release would put the
#    binaries at public URLs and the download password would protect nothing.
#    (Before 2026-07-10 the gate was just `github.ref_type == 'tag'`, and any tag published.)
gh workflow run Release --ref <branch>
gh run download <run-id> -D /tmp/dist

# 2. Generate the page. The JSON is {key: {name, size}} for the four packages.
#    Keys: windows_setup, windows_zip, linux_appimage, linux_targz.
python3 web/gen_page.py /tmp/index.html "$(cat /tmp/downloads.json)" 0.2.1

# 3. Upload to webvps. PACKAGES FIRST, then the page -- otherwise the page is live
#    for a few seconds linking to files that 404.
#      packages  -> /var/www/getstackbase/rollforge/downloads/
#      page      -> /var/www/getstackbase/rollforge/index.html
#    Always sha256sum both ends: the page links to these binaries.
```

Gotchas paid for in real time:

- **Tailscale SSH re-auth expires mid-session**, and an `scp` that hits it just *hangs* with no
  output -- for as long as you let it. If bytes are not moving, run a plain `ssh webvps true`
  first and complete the login URL it prints.
- **Actions runs the workflow file from the ref that triggered it.** Changing `release.yml` on
  `master` does nothing for an old ref: `v0.1.0` still carries the tag trigger *and* an ungated
  publish job. Deleting and re-pushing that tag, or dispatching against it, would publish public
  binaries. Do not re-push old tags.
- **To publish deliberately** (only if the binaries are meant to be public): set the repo variable
  `ROLLFORGE_PUBLISH_RELEASE=true`, then `gh workflow run Release --ref <tag>`. A dispatch against a
  tag is what makes `github.ref_type == 'tag'`; a branch dispatch never does.
- **No GitHub Release may carry assets while the repo is public.** The `v0.1.0` tag was pushed
  while the repo was private and its four assets stayed publicly downloadable afterwards; they
  were deleted on 2026-07-10. After deleting an asset, GitHub's CDN keeps serving a stale `200`
  -- re-check with a cache-buster (`?cb=$RANDOM`) or you will think the delete failed.

## Hosting

nginx on `webvps`, in `/etc/nginx/sites-available/getstackbase`:

- `/rollforge/` is public static HTML.
- `location ^~ /rollforge/downloads/` is HTTP basic auth against
  `/etc/nginx/.htpasswd-rollforge` (user `rollforge`). `^~` so the site's
  extension-blocklist regex location can never match first and skip the auth.
- The site's own `.htpasswd` (used by `sbdigital`) is a different file and untouched.

## Licence

RollForge links JUCE under its GPLv3 option, so distributing the binaries carries a GPLv3
obligation to offer the corresponding source. The footer carries a written offer
(`hello@getstackbase.com`) in place of a repository link. Do not remove it while the page
serves binaries.
