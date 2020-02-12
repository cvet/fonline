# Version 1.4.2 (July 29, 2019)

* 💡 Updated to UIkit 3.1.6

* ✅ Added `uk-heading` with size selectors from small - 2x large

* ✅ Added multiple new `uk-animation` snippets

* ✅ Added `uk-link-toggle`

* ❗️ Removed `uk-heading-primary`


# Version 1.4.1 (March 1, 2019)

* ⚙️️️ JS Snippet Option comments have been moved from the body into the descriptions
  * With [VSCode 1.31](https://code.visualstudio.com/updates/v1_31#_snippets-descriptions), support for multiline descriptions was added, so this is a more appropriate place to add them.
* Minor tweaks and fixes

## Contributions
* [Jakob Pless @pless84](https://github.com/pless84): 💡 Updated to UIkit 3.0.3 [PR [#18](https://github.com/dons20/UIKit-VSCode/issues/18)](https://github.com/dons20/UIKit-VSCode/pull/18)

# Version 1.4.0 (November 3, 2018)
* ⚙️️️ Modified main snippets to be global by default
  * There are many languages and associated files that the snippets wouldn't trigger in, and so this change will now allow `uk-` snippets to work in any file.
  * This change does not apply to the javascript-only snippets. Those will retain their original functionality within `javascript` files to avoid clutter.
  * To prevent this from occurring, simply disable the extension for that workspace, or until you need to re-activate it.
  * If this change comes with any issues or inconveniences, let me know on the [issues](https://github.com/dons20/UIKit-VSCode/issues) page!
* 💡 Updated to UIkit 3.0.0 RC 20
* ✅ Added `uk-divider-vertical` to snippets
* ⚙️️️️️ Small modifications to `uk-text-color` and other snippets

# Version 1.3.1 (August 10, 2018)
* 🔧 Fixed **README** formatting issues on marketplace

# Version 1.3.0 (August 10, 2018)
* 💡 Updated to UIkit 3.0.0 RC 11

* 💡 Updated formatting of the **README** to have collapsible sections by default
  * Collapsed sections aren't able to be jumped to by hyperlink until they are revealed in their respective categories. (e.g. Clicking `Container` in the Table of Contents won't jump to that section until you expand `List of Commands (HTML/PHP)` further below)

* 💡 Updated **CHANGELOG** formatting to be more spaced and have easily identifiable emojis 🌟

* 💡 Updated descriptions for various snippets

* ✅ Added missing links to documentation in the **README**

* ✅ Added snippet choices to clear up redundant ones (e.g. `uk-$-css-import`)

* ✅ Added image component snippets for both HTML and JS (e.g. `uk-img`)

* ✅ Added filter component snippets (e.g. `uk-filter`)

* ✅ Added seperate snippets for Height, Leader, SVG and Video components in both HTML and JS where needed (e.g. `uk-svg`, `uk-video`, etc.)

* 🔧 Fixed minor errors in a couple snippets

# Version 1.2.2 (April 20, 2018)
* 💡 Updated to UIkit 3.0.0 beta 42

* 💡 Updated and cleaned up many snippets

* ✅ Added slider component snippets (`uk-slider` and more)

* ✅ Added slideshow component snippets (`uk-slideshow` and more)

* ✅ Added thumbnav component snippets (`uk-thumbnav` and more)

* ❗️ Removed a number of snippets based on changes from beta 31

* ❗️ Removed JQuery import from snippets


# Version 1.2.0 (September 5, 2017)
* 💡 Updated a large number of HTML & JS snippets to now use snippet choices in VSCode 1.15+. This allows more variety within a single snippet, and so many redundancies have been ❗️ removed. 
  * Please note that a few of the snippet choices will be inactive/slightly bugged until the August VSCode update [due to a bug which has been fixed](https://github.com/Microsoft/vscode/issues/31599) for the next build.
  * A large number of snippets have recieved changes or minor tweaks. There are too many to list in this changelog, but any snippets which include multiple options will now utilize this new system.

* ✅ Added new method snippets to JS snippets. (e.g. `uk-modal-methods`, `uk-drop-methods`, etc.)

* 🔧 Fixed a few broken snippets

## Contributions
  * [Ramzi Akremi (@rakr)](https://github.com/rakr): ✅ Added support for embedded elixir [PR [#9](https://github.com/dons20/UIKit-VSCode/issues/9)](https://github.com/dons20/UIKit-VSCode/pull/9)

# Version 1.1.4 (August 21, 2017)
* 🔧 Fixed invalid icon display in marketplace

# Version 1.1.3 (August 21, 2017)
* 💡 Updated relevant snippets to UIkit 3.0.0 beta 30

* ✅ Added lightbox component snippets (`uk-lightbox` and more)

* ✅ Added video component snippets (`uk-utility-video`)

* ✅ Added support for PUG files

* ✅ Added `uk-alert-methods` snippet for JS files

* ⚙️ Modified `uk-modal-center` to use `uk-margin-auto-vertical` class due to deprecated *center* parameter

* ❗️ Removed `uk-modal-caption` snippet (functionality now in `uk-lightbox` instead)

* ⚠️ Moved contribution guidelines to a seperate file to remove initial clutter from `README` file. View here: 
[How to contribute](https://github.com/dons20/UIKit-VSCode/blob/master/./CONTRIBUTION.md)

# Version 1.1.2 (June 14, 2017)
* 💡 Updated relevant snippets to UIkit 3.0.0 beta 25

* ✅ Added size modifier snippet to tile component (`uk-tile-size-modifier`)

* ✅ Added body text modifier snippet to link component (`uk-link-text`)

# Version 1.1.1 (May 31, 2017)
* 💡 Updated relevant snippets to UIkit 3.0.0 beta 24

* 💡 Updated `uk-form-sample` to include the `uk-range` element

* ✅ Added support for `twig` files

* ✅ Added Parallax component snippets (`uk-parallax` etc.)

* ✅ Added Grid Parallax component (`uk-grid-parallax`)

* ✅ Added Marker component (`uk-marker`)

* ✅ Added `uk-hidden-touch` and `uk-hidden-notouch`

* ✅ Added `uk-transform-center`

# Version 1.1.0 (April 28, 2017)
* 💡 Updated relevant snippets to UIkit 3.0.0 beta 22

* ✅ Added box-shadow bottom (`uk-utility-box-shadow-bottom`)

* ✅ Added countdown component snippets (`uk-countdown` and more)

* ✅ Added divider & justify modifier snippet for tables (`uk-table-divider` & `uk-table-justify`)

* ✅ Added the remaining attribute/class-only snippets. Attribute/Class-only snippets are indicated by [Attribute] & [Class] respectively in their description.

* ⚙️ Changed `uk-table-center` to `uk-table-middle`

* 🔧 Fixed a few inconsistensies in html snippets & readme descriptions

# Version 1.0.13 (April 13, 2017)
* ✅ Added Javascript-only snippets for multiple components (check the readme for a detailed list)

* ⚙️ Started adding attribute/class-only versions for many snippets (will be finished by 1.1.0)

# Version 1.0.12 (April 11, 2017)
* 💡 Updated relevant snippets to UIKit 3.0.0 beta 21

# Version 1.0.11 (April 7, 2017)
* ✅ Added breakpoint snippet for flex horizontal alignment (`uk-flex-horizontal-alignment-responsive`)

* 💡 Updated relevant snippets to UIKit 3.0.0 beta 20

# Version 1.0.10 (April 4, 2017)
* ✅ Added Utility Leader snippet (`uk-utility-leader`)

* ⚙️ Modified off-canvas snippets to match new format (Off-Canvas now requires wrapping the page into an extra div)

* 💡 Updated relevant snippets to UIKit 3.0.0 beta 19

* 💡 Updated slidenav snippet's button order

* 💡 Updated the margin-auto snippet

* 💡 Updated padding-remove snippet

# Version 1.0.9 (March 21, 2017)
* 💡 Updated jquery snippets to version 3.2.1

# Version 1.0.8 (March 19, 2017)
* 💡 Updated the descriptions of class-only snippets to include `[Class]` at the start for clarity

* 🔧 Fixed a few tab errors/inconsistensies

# Version 1.0.7 (March 12, 2017)
* ✅ Added "Match only one cell" snippet to grid (`uk-grid-match-cell`)

* ✅ Added CSS and JS import snippets for ease of updating existing files (`uk-$-css-import`, 
`uk-$-css-rtl-import`, `uk-$-js-import`)
* Changed `uk-background` snippet to `uk-background-default` to match [beta 17's change]
(https://getuikit.com/changelog)
* 💡 Updated a few more tab locations to make them more intutive (work in progress)

* 💡 Updated a few descriptions to specify that they are class-only snippets (WIP for version 1.1.0)

# Version 1.0.6 (March 10, 2017)
* 💡 Updated relevant snippets to UIKit 3.0.0-beta.18

* ✅ Added the `tile` component (`uk-tile-`)

* 🔧 Fixed a few tab positions in some elements

* ⚙️ Modified changlog style (again)

# Version 1.0.5 (March 3, 2017)
* ✅ Added RTL snippet (Triggered with `uk-$-rtl`)

* 💡 Updated basic template snippet (`uk-$`) to include `uikit-icons.min.js`

* 💡 Updated many existing snippets (such as `uk-container` & `uk-section`) to be better formatted when inserted

# Version 1.0.4 (March 2, 2017)
* 💡 Updated relevant snippets to UIKit 3.0.0-beta.16

# Version 1.0.3 (March 1, 2017)
* 💡 Updated relevant snippets to UIKit 3.0.0-beta.13

# Version 1.0.2 (March 1, 2017)
* ✅ Added support for `php` files (Must be between `<?php ?>` tags for snippets to show up)

# Version 1.0.1 (March 1, 2017)
* ⚙️ Minor tweaks to readme for consistency

# Version 1.0.0 (March 1, 2017)
* ✅ Initial support for UIKit beta 3.0