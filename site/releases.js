// The release page's list, from releases.json: the web server reads it live
// from the GitLab releases (see deploy/nginx/).
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

  // What each package is for, by its file name (see .gitlab-ci.yml).
  var PACKAGES = [
    {kind: 'windows', test: /-windows-x64\.zip$/, main: 'Download for Windows', item: 'Windows x64', note: '.zip'},
    {kind: 'deb', test: /\.deb$/, main: 'Download for Linux (.deb)', item: 'Linux .deb', note: 'Debian, Ubuntu'},
    {kind: 'rpm', test: /\.rpm$/, main: 'Download for Linux (.rpm)', item: 'Linux .rpm', note: 'Fedora, openSUSE, RHEL'}
  ];

  function packageOf(name) {
    for (var i = 0; i < PACKAGES.length; i++) {
      if (PACKAGES[i].test.test(name)) return PACKAGES[i];
    }
    return {kind: '', main: 'Download', item: name, note: ''};
  }

  // The visitor's system, as far as the browser tells: Linux distributions
  // with .rpm packages sometimes name themselves in the user agent.
  function visitorKind() {
    var ua = navigator.userAgent || '';
    var platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
    if (/windows/i.test(ua) || /^win/i.test(platform)) return 'windows';
    if (/linux/i.test(platform + ' ' + ua) && !/android/i.test(ua)) {
      return /fedora|red ?hat|rhel|centos|rocky|alma|suse/i.test(ua) ? 'rpm' : 'deb';
    }
    return 'windows';
  }

  function label(parent, key, text) {
    var span = parent.appendChild(el('span', '', text));
    span.setAttribute('data-i18n', key);
    return span;
  }

  // The button downloads the visitor's package; the arrow beside it drops
  // down all of the release's packages.
  function downloads(release, primary) {
    var group = el('div', 'dl-group');
    var files = release.files.map(function (file) { return {file: file, kind: packageOf(file.name)}; });
    var mine = files[0];
    files.forEach(function (entry) { if (entry.kind.kind === visitorKind()) mine = entry; });

    var main = group.appendChild(el('a', 'btn' + (primary ? ' primary' : '')));
    main.href = mine.file.url;
    main.setAttribute('download', mine.file.name);
    main.title = mine.file.name;
    label(main, 'rel.dl.' + (mine.kind.kind || 'other'), mine.kind.main);

    if (files.length > 1) {
      var more = group.appendChild(el('details', 'dropdown dl-more'));
      var arrow = more.appendChild(el('summary', 'btn' + (primary ? ' primary' : '')));
      arrow.setAttribute('aria-label', 'All packages');
      arrow.title = 'All packages';
      arrow.appendChild(el('span', 'arrow')).appendChild(el('i'));
      var menu = more.appendChild(el('div', 'menu'));
      files.forEach(function (entry) {
        var item = menu.appendChild(el('a', 'btn'));
        item.href = entry.file.url;
        item.setAttribute('download', entry.file.name);
        item.title = entry.file.name;
        label(item, 'rel.pkg.' + (entry.kind.kind || 'other'), entry.kind.item);
        if (entry.kind.note) label(item, 'rel.note.' + entry.kind.kind, entry.kind.note).className = 'pkg-note';
      });
    }
    return group;
  }

  // The status line: loading, nothing yet, or the list could not be read.
  function status(key, text) {
    var line = document.getElementById('release-status');
    line.setAttribute('data-i18n', key);
    line.textContent = text;
    delete line.dataset.en;
    line.hidden = !key;
    if (window.glimpseRetranslate) window.glimpseRetranslate();
  }

  function show(releases) {
    if (!releases.length) {
      status('rel.none', 'No releases yet.');
      return;
    }
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
      head.appendChild(downloads(release, index === 0));
      card.appendChild(notes(release.notes, release.version));
      listBox.appendChild(card);
    });
    status('', '');
  }

  // Read live from GitLab by the server on every visit (cached for a minute).
  fetch('releases.json', {cache: 'no-cache'})
    .then(function (response) {
      if (!response.ok) throw new Error(response.status);
      return response.json();
    })
    .then(function (releases) {
      show((releases || []).filter(function (r) { return r.files && r.files.length; }));
    })
    .catch(function () {
      status('rel.error', 'The releases cannot be read right now. Try GitHub or GitLab above.');
    });
})();
