ScanDialog::ScanDialog() {
  scanDialog = this;

  onClose(&Application::quit);
  layout.setMargin(5);
  pathEdit.onActivate([&] { refresh(); });
  refreshButton.setIcon(Icon::Action::Refresh).setBordered(false).onActivate([&] {
    pathEdit.setText(settings["daedalus/Path"].text());
    refresh();
  });
  homeButton.setIcon(Icon::Go::Home).setBordered(false).onActivate([&] {
    pathEdit.setText(Path::user());
    refresh();
  });
  upButton.setIcon(Icon::Go::Up).setBordered(false).onActivate([&] {
    pathEdit.setText(Location::dir(settings["daedalus/Path"].text()));
    refresh();
  });
  scanList.onActivate([&] { activate(); });
  selectAllButton.setText("Select All").onActivate([&] {
    for(auto& item : scanList.items()) {
      if(item.checkable()) item.setChecked(true);
    }
  });
  unselectAllButton.setText("Unselect All").onActivate([&] {
    for(auto& item : scanList.items()) {
      if(item.checkable()) item.setChecked(false);
    }
  });
  settingsButton.setText("Settings ...").onActivate([&] {
    settingsDialog->setCentered(*this);
    settingsDialog->setVisible();
    settingsDialog->setFocused();
  });
  importButton.setText("Import ...").onActivate([&] { import(); });

  setTitle("daedalus");
  setSize({800, 480});
  setCentered();
}

auto ScanDialog::show() -> void {
  setVisible();
  pathEdit.setText(settings["daedalus/Path"].text());
  refresh();
}

auto ScanDialog::refresh() -> void {
  scanList.reset();

  auto pathname = pathEdit.text().transform("\\", "/").trimRight("/").append("/");
  if(!directory::exists(pathname)) return;

  settings["daedalus/Path"].setValue(pathname);
  pathEdit.setText(pathname);
  auto contents = directory::icontents(pathname);

  for(auto& name : contents) {
    if(!name.endsWith("/")) continue;
    if(gamePakType(Location::suffix(name))) continue;
    scanList.append(ListViewItem().setIcon(Icon::Emblem::Folder).setText(name.trimRight("/")));
  }

  for(auto& name : contents) {
    if(name.endsWith("/")) continue;
    if(!gameRomType(Location::suffix(name).downcase())) continue;
    scanList.append(ListViewItem().setCheckable().setIcon(Icon::Emblem::File).setText(name));
  }

  Application::processEvents();
  scanList.setFocused();
}

auto ScanDialog::activate() -> void {
  if(auto item = scanList.selected()) {
    string location{settings["daedalus/Path"].text(), item.text()};
    if(directory::exists(location) && !gamePakType(Location::suffix(location))) {
      pathEdit.setText(location);
      refresh();
    }
  }
}

auto ScanDialog::import() -> void {
  string_vector filenames;
  for(auto& item : scanList.items()) {
    if(item.checked()) {
      filenames.append(string{settings["daedalus/Path"].text(), item.text()});
    }
  }

  if(!filenames) {
    MessageDialog().setParent(*this).setText("Nothing selected to import.").error();
    return;
  }

  setVisible(false);
  importDialog->run(filenames);
}

auto ScanDialog::gamePakType(const string& type) -> bool {
  return type == ".sys"
  || type == ".sfc"
  || type == ".bs"
  || type == ".st";
}

auto ScanDialog::gameRomType(const string& type) -> bool {
  return type == ".zip"
  || type == ".sfc" || type == ".smc"
  || type == ".bs"
  || type == ".st";
}