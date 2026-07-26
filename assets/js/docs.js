(function () {
  "use strict";

  var body = document.body;
  var navigationToggle = document.querySelector("[data-nav-toggle]");
  var navigationScrim = document.querySelector("[data-nav-scrim]");
  var themeToggle = document.querySelector("[data-theme-toggle]");
  var searchDialog = document.querySelector("[data-search-dialog]");
  var searchInput = document.querySelector("[data-search-input]");
  var searchStatus = document.querySelector("[data-search-status]");
  var searchResults = document.querySelector("[data-search-results]");
  var searchIndex = null;
  var searchIndexPromise = null;

  function setNavigation(open) {
    body.classList.toggle("nav-open", open);
    if (navigationToggle) {
      navigationToggle.setAttribute("aria-expanded", String(open));
      navigationToggle.setAttribute("aria-label", open ? "Close navigation" : "Open navigation");
    }
  }

  if (navigationToggle) {
    navigationToggle.addEventListener("click", function () {
      setNavigation(!body.classList.contains("nav-open"));
    });
  }

  if (navigationScrim) {
    navigationScrim.addEventListener("click", function () {
      setNavigation(false);
    });
  }

  document.querySelectorAll(".docs-navigation a").forEach(function (link) {
    link.addEventListener("click", function () {
      setNavigation(false);
    });
  });

  function updateThemeButton() {
    if (!themeToggle) {
      return;
    }
    var isDark = document.documentElement.dataset.theme === "dark";
    themeToggle.setAttribute("aria-label", isDark ? "Use light theme" : "Use dark theme");
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
      button.textContent = "Copy";
      button.addEventListener("click", function () {
        navigator.clipboard.writeText(code.textContent || "").then(function () {
          button.textContent = "Copied";
          window.setTimeout(function () {
            button.textContent = "Copy";
          }, 1400);
        });
      });
      pre.appendChild(button);
    });
  }

  function tokenize(query) {
    var matches = query.toLowerCase().match(/[a-z0-9_][a-z0-9_.:+/?<>-]*/g) || [];
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
      searchIndexPromise = fetch(searchDialog.dataset.searchUrl, { credentials: "same-origin" })
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

  function addPostingScores(scores, matches, postings, multiplier) {
    postings.forEach(function (posting) {
      var documentIndex = posting[0];
      scores.set(documentIndex, (scores.get(documentIndex) || 0) + posting[1] * multiplier);
      matches.set(documentIndex, (matches.get(documentIndex) || 0) + 1);
    });
  }

  function searchDocuments(index, query) {
    var tokens = tokenize(query);
    if (!tokens.length) {
      return [];
    }
    var scores = new Map();
    var matches = new Map();
    var allTerms = Object.keys(index.terms);

    tokens.forEach(function (token) {
      if (index.terms[token]) {
        addPostingScores(scores, matches, index.terms[token], 1);
        return;
      }
      if (token.length < 3) {
        return;
      }
      var prefixMatches = 0;
      for (var termIndex = 0; termIndex < allTerms.length && prefixMatches < 32; termIndex += 1) {
        var term = allTerms[termIndex];
        if (term.indexOf(token) === 0) {
          addPostingScores(scores, matches, index.terms[term], 0.55);
          prefixMatches += 1;
        }
      }
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
        return result.matches >= Math.max(1, tokens.length - 1);
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
      meta.textContent = result.document.section_title + " / " + result.document.diataxis;
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
      searchStatus.textContent = "Type at least 2 characters.";
      searchResults.replaceChildren();
      return;
    }
    loadSearchIndex()
      .then(function (index) {
        var results = searchDocuments(index, query);
        searchStatus.textContent = results.length ? results.length + " result" + (results.length === 1 ? "" : "s") : "No matching documents.";
        renderSearchResults(results);
      })
      .catch(function () {
        searchStatus.textContent = "Search is temporarily unavailable.";
        searchResults.replaceChildren();
      });
  }

  function openSearch() {
    setNavigation(false);
    if (!searchDialog.open) {
      searchDialog.showModal();
    }
    loadSearchIndex().catch(function () {
      searchStatus.textContent = "Search is temporarily unavailable.";
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
      setNavigation(false);
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
}());
