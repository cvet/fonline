(function () {
  "use strict";

  var body = document.body;
  var navigationToggle = document.querySelector("[data-nav-toggle]");
  var navigationScrim = document.querySelector("[data-nav-scrim]");
  var navigationSidebar = document.querySelector(".docs-sidebar");
  var mobileNavigation = window.matchMedia("(max-width: 900px)");
  var themeToggle = document.querySelector("[data-theme-toggle]");
  var searchDialog = document.querySelector("[data-search-dialog]");
  var searchInput = document.querySelector("[data-search-input]");
  var searchStatus = document.querySelector("[data-search-status]");
  var searchResults = document.querySelector("[data-search-results]");
  var searchIndex = null;
  var searchIndexPromise = null;
  var pageLocale = document.documentElement.lang === "ru" ? "ru" : "en";

  function ui(english, russian) {
    return pageLocale === "ru" ? russian : english;
  }

  function navigationFocusables() {
    if (!navigationSidebar || !navigationToggle) {
      return [];
    }
    return [navigationToggle].concat(
      Array.from(
        navigationSidebar.querySelectorAll(
          "a[href], button:not([disabled]), summary, input:not([disabled]), [tabindex]:not([tabindex='-1'])"
        )
      )
    );
  }

  function updateNavigationAccessibility() {
    if (!navigationSidebar) {
      return;
    }
    var hidden = mobileNavigation.matches && !body.classList.contains("nav-open");
    navigationSidebar.inert = hidden;
    if (hidden) {
      navigationSidebar.setAttribute("aria-hidden", "true");
    } else {
      navigationSidebar.removeAttribute("aria-hidden");
    }
  }

  function setNavigation(open, restoreFocus) {
    var willOpen = mobileNavigation.matches && open;
    var wasOpen = body.classList.contains("nav-open");
    body.classList.toggle("nav-open", willOpen);
    if (navigationToggle) {
      navigationToggle.setAttribute("aria-expanded", String(willOpen));
      navigationToggle.setAttribute(
        "aria-label",
        willOpen
          ? ui("Close navigation", "Закрыть навигацию")
          : ui("Open navigation", "Открыть навигацию")
      );
    }
    updateNavigationAccessibility();
    if (willOpen && navigationSidebar) {
      var firstSidebarControl = navigationSidebar.querySelector(
        "button:not([disabled]), a[href], summary, input:not([disabled]), [tabindex]:not([tabindex='-1'])"
      );
      if (firstSidebarControl) {
        firstSidebarControl.focus();
      }
    } else if (wasOpen && restoreFocus && navigationToggle) {
      navigationToggle.focus();
    }
  }

  if (navigationToggle) {
    navigationToggle.addEventListener("click", function () {
      setNavigation(!body.classList.contains("nav-open"), true);
    });
  }

  if (navigationScrim) {
    navigationScrim.addEventListener("click", function () {
      setNavigation(false, true);
    });
  }

  document.querySelectorAll(".docs-navigation a").forEach(function (link) {
    link.addEventListener("click", function () {
      setNavigation(false);
    });
  });

  mobileNavigation.addEventListener("change", function () {
    setNavigation(false);
  });
  updateNavigationAccessibility();

  function updateThemeButton() {
    if (!themeToggle) {
      return;
    }
    var isDark = document.documentElement.dataset.theme === "dark";
    themeToggle.setAttribute(
      "aria-label",
      isDark
        ? ui("Use light theme", "Использовать светлую тему")
        : ui("Use dark theme", "Использовать тёмную тему")
    );
  }

  if (themeToggle) {
    updateThemeButton();
    themeToggle.addEventListener("click", function () {
      var nextTheme = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
      document.documentElement.dataset.theme = nextTheme;
      localStorage.setItem("fonline-docs-theme", nextTheme);
      updateThemeButton();
    });
  }

  function buildTableOfContents() {
    var tableOfContents = document.querySelector("[data-page-toc]");
    var linksContainer = document.querySelector("[data-page-toc-links]");
    if (!tableOfContents || !linksContainer) {
      return;
    }
    var headings = Array.from(document.querySelectorAll(".docs-content h2[id], .docs-content h3[id]"));
    if (headings.length < 2) {
      return;
    }
    var list = document.createElement("ol");
    headings.forEach(function (heading) {
      var item = document.createElement("li");
      var link = document.createElement("a");
      item.className = heading.tagName === "H3" ? "is-level-3" : "is-level-2";
      link.href = "#" + heading.id;
      link.textContent = heading.textContent;
      item.appendChild(link);
      list.appendChild(item);
    });
    linksContainer.appendChild(list);
    tableOfContents.hidden = false;
  }

  function addCopyButtons() {
    document.querySelectorAll(".docs-content pre > code").forEach(function (code) {
      var pre = code.parentElement;
      if (!pre || pre.querySelector(".copy-code")) {
        return;
      }
      var button = document.createElement("button");
      button.type = "button";
      button.className = "copy-code";
      button.textContent = ui("Copy", "Копировать");
      button.addEventListener("click", function () {
        navigator.clipboard.writeText(code.textContent || "").then(function () {
          button.textContent = ui("Copied", "Скопировано");
          window.setTimeout(function () {
            button.textContent = ui("Copy", "Копировать");
          }, 1400);
        });
      });
      pre.appendChild(button);
    });
  }

  function updateScrollableTables() {
    document.querySelectorAll(".docs-content table").forEach(function (table) {
      var isScrollable = table.scrollWidth > table.clientWidth + 1;
      if (isScrollable) {
        table.tabIndex = 0;
        table.dataset.docsScrollable = "true";
      } else if (table.dataset.docsScrollable === "true") {
        table.removeAttribute("tabindex");
        delete table.dataset.docsScrollable;
      }
    });
  }

  function tokenize(query) {
    var matches = query.toLowerCase().match(/[\p{L}\p{N}_][\p{L}\p{N}_.:+/?<>-]*/gu) || [];
    var tokens = [];
    matches.forEach(function (match) {
      var normalized = match.replace(/^[./:+?<>-]+|[./:+?<>-]+$/g, "");
      if (normalized.length >= 2 && tokens.indexOf(normalized) === -1) {
        tokens.push(normalized);
      }
    });
    return tokens;
  }

  function loadSearchIndex() {
    if (searchIndex) {
      return Promise.resolve(searchIndex);
    }
    if (!searchIndexPromise) {
      var searchUrl = pageLocale === "ru" && searchDialog.dataset.searchUrlRu
        ? searchDialog.dataset.searchUrlRu
        : searchDialog.dataset.searchUrl;
      searchIndexPromise = fetch(searchUrl, { credentials: "same-origin" })
        .then(function (response) {
          if (!response.ok) {
            throw new Error("Unable to load search index");
          }
          return response.json();
        })
        .then(function (index) {
          searchIndex = index;
          return index;
        });
    }
    return searchIndexPromise;
  }

  function collectPostingScores(tokenScores, postings, multiplier) {
    postings.forEach(function (posting) {
      var documentIndex = posting[0];
      tokenScores.set(documentIndex, Math.max(tokenScores.get(documentIndex) || 0, posting[1] * multiplier));
    });
  }

  // The index is a plain JSON object, so the lowercased query "__proto__" or "constructor" would
  // otherwise resolve to an Object.prototype member and fail as a non-array posting list
  function termPostings(terms, token) {
    return Object.prototype.hasOwnProperty.call(terms, token) ? terms[token] : null;
  }

  function searchDocuments(index, query) {
    var tokens = tokenize(query);
    if (!tokens.length) {
      return [];
    }
    var scores = new Map();
    var matches = new Map();
    var allTerms = Object.keys(index.terms);
    var effectiveTokenCount = 0;

    tokens.forEach(function (token) {
      var tokenScores = new Map();
      var exactPostings = termPostings(index.terms, token);
      if (exactPostings) {
        collectPostingScores(tokenScores, exactPostings, 1);
      } else if (token.length >= 3) {
        var prefixMatches = 0;
        for (var termIndex = 0; termIndex < allTerms.length && prefixMatches < 32; termIndex += 1) {
          var term = allTerms[termIndex];
          if (term.indexOf(token) === 0) {
            collectPostingScores(tokenScores, termPostings(index.terms, term), 0.55);
            prefixMatches += 1;
          }
        }
      }
      if (!tokenScores.size) {
        return;
      }
      effectiveTokenCount += 1;
      tokenScores.forEach(function (tokenScore, documentIndex) {
        scores.set(documentIndex, (scores.get(documentIndex) || 0) + tokenScore);
        matches.set(documentIndex, (matches.get(documentIndex) || 0) + 1);
      });
    });

    var normalizedQuery = query.trim().toLowerCase();
    return Array.from(scores.entries())
      .map(function (entry) {
        var documentIndex = entry[0];
        var document = index.documents[documentIndex];
        var score = entry[1];
        if (document.title.toLowerCase().indexOf(normalizedQuery) !== -1) {
          score += 80;
        }
        if (document.id.toLowerCase() === normalizedQuery) {
          score += 120;
        }
        return { document: document, score: score, matches: matches.get(documentIndex) || 0 };
      })
      .filter(function (result) {
        return result.matches >= Math.max(1, Math.ceil(effectiveTokenCount * 0.6));
      })
      .sort(function (left, right) {
        return right.matches - left.matches || right.score - left.score || left.document.title.localeCompare(right.document.title);
      })
      .slice(0, 12);
  }

  function renderSearchResults(results) {
    searchResults.replaceChildren();
    results.forEach(function (result) {
      var item = document.createElement("li");
      var link = document.createElement("a");
      var title = document.createElement("span");
      var meta = document.createElement("span");
      var summary = document.createElement("span");
      item.className = "search-result";
      link.href = result.document.url;
      title.className = "search-result-title";
      title.textContent = result.document.title;
      meta.className = "search-result-meta";
      meta.textContent = result.document.section_title + " / " + result.document.diataxis_title;
      summary.className = "search-result-summary";
      summary.textContent = result.document.summary || result.document.path;
      link.appendChild(title);
      link.appendChild(meta);
      link.appendChild(summary);
      item.appendChild(link);
      searchResults.appendChild(item);
    });
  }

  function runSearch() {
    var query = searchInput.value.trim();
    if (query.length < 2) {
      searchStatus.textContent = ui("Type at least 2 characters.", "Введите не менее 2 символов.");
      searchResults.replaceChildren();
      return;
    }
    loadSearchIndex()
      .then(function (index) {
        var results = searchDocuments(index, query);
        searchStatus.textContent = results.length
          ? ui(
              results.length + " result" + (results.length === 1 ? "" : "s"),
              "Найдено: " + results.length
            )
          : ui("No matching documents.", "Совпадений нет.");
        renderSearchResults(results);
      })
      .catch(function () {
        searchStatus.textContent = ui(
          "Search is temporarily unavailable.",
          "Поиск временно недоступен."
        );
        searchResults.replaceChildren();
      });
  }

  function openSearch() {
    setNavigation(false);
    if (!searchDialog.open) {
      searchDialog.showModal();
    }
    loadSearchIndex().catch(function () {
      searchStatus.textContent = ui(
        "Search is temporarily unavailable.",
        "Поиск временно недоступен."
      );
    });
    window.setTimeout(function () {
      searchInput.focus();
      searchInput.select();
    }, 0);
  }

  if (searchDialog && searchInput && searchStatus && searchResults) {
    document.querySelectorAll("[data-search-open]").forEach(function (button) {
      button.addEventListener("click", openSearch);
    });
    document.querySelector("[data-search-close]").addEventListener("click", function () {
      searchDialog.close();
    });
    searchDialog.addEventListener("click", function (event) {
      if (event.target === searchDialog) {
        searchDialog.close();
      }
    });
    searchInput.addEventListener("input", runSearch);
  }

  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape") {
      if (searchDialog && searchDialog.open) {
        searchDialog.close();
      }
      setNavigation(false, true);
    }
    if (event.key === "Tab" && mobileNavigation.matches && body.classList.contains("nav-open")) {
      var focusables = navigationFocusables();
      var currentIndex = focusables.indexOf(document.activeElement);
      if (focusables.length) {
        event.preventDefault();
        var direction = event.shiftKey ? -1 : 1;
        var nextIndex = currentIndex + direction;
        if (currentIndex === -1) {
          nextIndex = event.shiftKey ? focusables.length - 1 : 0;
        }
        nextIndex = (nextIndex + focusables.length) % focusables.length;
        focusables[nextIndex].focus();
      }
    }
    var target = event.target;
    var isTyping = target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement || target.isContentEditable;
    if (!isTyping && (event.key === "/" || ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k"))) {
      event.preventDefault();
      openSearch();
    }
  });

  buildTableOfContents();
  addCopyButtons();
  updateScrollableTables();
  window.addEventListener("resize", updateScrollableTables);
}());
