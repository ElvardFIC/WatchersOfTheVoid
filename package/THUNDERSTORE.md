# Publishing Watchers of the Void

The owner requested the 2162 build on 27 October 2026. Its complete offline
suite and archive verification are recorded in agent/TESTLOG.md. Publication
requires an explicit request.

- Team/namespace: **ElvardFIC**. Select or create this team in Thunderstore before upload.
  The visible owner comes from the upload team, not an author field in manifest.json.
- Package name: **WatchersOfTheVoid**; version: **0.9.2162**.
- Internal DLL build / network protocol: **2217**; the package version is **0.9.2162**.
- Expected dependency identifier after publication: `ElvardFIC-WatchersOfTheVoid-0.9.2162`.
- When package preparation is explicitly requested, run `package/build_current.ps1`
  without reusing older regression results for the changed production source. The final
  intended archive name remains `dist/ElvardFIC-WatchersOfTheVoid-0.9.2162.zip`.
- Preview README and icon before submitting. The supplied cover is retained at
  `assets/branding/cover.jpg`; the package icon is its 256x256 PNG conversion.
- Website: https://github.com/ElvardFIC/WatchersOfTheVoid.
  The mod also builds on MultiVoidKindaPlus, as acknowledged in the package Credits.
- If version 0.9.2162 already exists in this namespace/package, prepare a new version
  before upload; do not replace an already published version with different bytes.

Prepared locally only. No upload, team creation or publication has been performed.
Official package requirements: https://wiki.thunderstore.io/mods/creating-a-package

Release notes for the version description: use the 0.9.2162 section of CHANGELOG_PLUS.md.
README.md is the full page description; description.txt is shared by the package
manifest and the GitHub repository's short description.
Review RELEASE.md for verification scope and remaining reports before submission.
The ZIP and SHA256SUMS-current.txt in the workspace dist/ directory are the delivery files.
