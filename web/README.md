# The RollForge web page

`gen_page.py` generates `index.html` for <https://getstackbase.com/rollforge>.

The **user manual on that page is generated from `src/ui/AboutView.cpp`** — the app's own
in-app Help text — so the page cannot drift from the app. The feature cards and the layout
are hand-written here; everything under "User manual" is not.

## Rebuild and publish

```bash
# 1. Build the packages WITHOUT publishing a GitHub Release.
#    release.yml gates its publish job on `github.ref_type == 'tag'`, so a manual dispatch
#    builds all four packages and publishes none. This matters: the repo is public, so a
#    Release would put the binaries at public URLs and the download password would protect
#    nothing.
gh workflow run Release --ref <branch>
gh run download <run-id> -D /tmp/dist

# 2. Generate the page. The JSON is {key: {name, size}} for the four packages.
python3 web/gen_page.py /tmp/index.html "$(cat /tmp/downloads.json)" 0.2.0

# 3. Upload to webvps:
#      page      -> /var/www/getstackbase/rollforge/index.html
#      packages  -> /var/www/getstackbase/rollforge/downloads/
#    Always sha256sum both ends: the page links to these binaries.
```

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
