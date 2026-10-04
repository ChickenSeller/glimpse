// The release list (releases.json, written when the page is deployed; see
// the pages job in .gitlab-ci.yml) and the "From this site" download, which
// leads to it. Both stay hidden without releases.
(function () {
  function el(tag, className, text) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  // Release notes are plain text: "- " starts a list item (indented lines
  // continue it), other lines are short headings such as "New" or "Fixes".
  // The first line, the release's own name, is left out.
  function notes(text, version) {
    var box = el('div', 'notes');
    var list = null;
    var lines = (text || '').split(/\r?\n/);
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      if (!line.trim() || line.trim() === 'Glimpse ' + version) {
        list = null;
        continue;
      }
      if (/^\s*-\s/.test(line)) {
        if (!list) list = box.appendChild(el('ul'));
        list.appendChild(el('li', '', line.replace(/^\s*-\s+/, '')));
      } else if (/^\s{2,}/.test(line) && list && list.lastChild) {
        list.lastChild.textContent += ' ' + line.trim();
      } else {
        // Sentences are text; short lines without a full stop are headings.
        list = null;
        box.appendChild(el('p', /\.$/.test(line.trim()) ? 'notes-text' : 'notes-head', line.trim()));
      }
    }
    return box;
  }

  function show(releases) {
    if (!releases.length) return;
    var listBox = document.getElementById('release-list');
    releases.forEach(function (release, index) {
      var card = el('article', 'release' + (index === 0 ? ' latest' : ''));
      var head = card.appendChild(el('div', 'release-head'));
      var title = head.appendChild(el('h3', '', release.version));
      if (index === 0) {
        var badge = title.appendChild(el('span', 'badge', 'Latest'));
        badge.setAttribute('data-i18n', 'rel.latest');
      }
      head.appendChild(el('span', 'release-date', release.date || ''));
      release.files.forEach(function (file) {
        var link = head.appendChild(el('a', 'btn' + (index === 0 ? ' primary' : '')));
        link.href = file.url;
        link.setAttribute('download', file.name);
        link.title = file.name;
        var label = link.appendChild(el('span', '', 'Download'));
        label.setAttribute('data-i18n', 'rel.download');
      });
      card.appendChild(notes(release.notes, release.version));
      listBox.appendChild(card);
    });

    document.getElementById('releases').hidden = false;
    document.getElementById('nav-releases').hidden = false;
    // "From this site" opens the list to pick a version from.
    var fromSite = document.getElementById('download-site');
    fromSite.hidden = false;
    fromSite.addEventListener('click', function () {
      document.getElementById('download').open = false;
    });
    if (window.glimpseRetranslate) window.glimpseRetranslate();
  }

  fetch('releases.json', {cache: 'no-cache'})
    .then(function (response) { return response.ok ? response.json() : []; })
    .then(function (releases) {
      show((releases || []).filter(function (r) { return r.files && r.files.length; }));
    })
    .catch(function () {});
})();
