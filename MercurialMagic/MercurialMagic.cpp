#include <MercurialMagic.hpp>

unique_pointer<Program> program;

Program::Program(string_vector args) {
  program = this;
  Application::onMain({&Program::main, this});

  zipIndex = 0;

  args.takeLeft();  //ignore program location in argument parsing

  if(args) {
    packPath = args.takeLeft();
  } else {
    packPath = BrowserDialog()
    .setTitle("Load MSU-1 Pack")
    .setPath(Path::program())
    .setFilters(string{"MSU-1 Pack|*.msu1"})
    .openFile();
  }

  if(!packPath) quit();

  pack.open(packPath);

  if(!validate()) error("This is not a valid MSU-1 pack.");

  usesPatch = fetch(patch);

  if(usesPatch) {
    if(args) {
      romPath = args.takeLeft();
    } else {
      romPath = BrowserDialog()
      .setTitle("Load Super Famicom ROM")
      .setPath(Path::real(packPath))
      .setFilters(string{"Super Famicom ROM|*.sfc"})
      .openFile();
    }
  }

  if(usesPatch && !romPath) quit();
  if(usesPatch) patch.source(romPath);

  exportMethod = ExportMethod::GamePak;
  new ExportSettings;
  exportSettings->setVisible(true);
}

auto Program::validate() -> bool {
  bool valid;

  valid = false;
  for(auto& file : pack.file) {
    if(file.name == "msu1.rom") { valid = true; break; }
  }
  if(!valid) return false;

  valid = false;
  for(auto& file : pack.file) {
    if(file.name == "program.rom" || file.name == "patch.bps") { valid = true; break; }
  }
  if(!valid) return false;

  return true;
}

auto Program::fetch(string_view name) -> maybe<Decode::ZIP::File> {
  for(auto& file : pack.file) {
    if(file.name.match(name)) return file;
  }
  return nothing;
}

auto Program::fetch(bpspatch& patch) -> bool {
  if(auto file = fetch("patch.bps")) {
    patchContents = pack.extract(file());
    patch.modify(patchContents.data(), patchContents.size());
    return true;
  }
  return false;
}

auto Program::beginExport() -> void {
  uint patchResult;

  switch(exportMethod) {

  case ExportMethod::GamePak: {
    destination = {Location::dir(packPath), Location::prefix(packPath), ".sfc", "/"};

    directory::create(destination);

    if(usesPatch) {
      patch.target({destination, "program.rom"});
      patchResult = patch.apply();
    }

    break;
  }

  case ExportMethod::SD2SNES: {
    destination = {Location::dir(packPath), "SD2SNES/"};

    directory::create(destination);

    if(usesPatch) {
      patch.target({destination, Location::prefix(packPath), ".sfc"});
      patchResult = patch.apply();
    }

    static string_vector roms = {
      "program.rom",
      "data.rom",
      "slot-*.rom",
      "*.boot.rom",
      "*.program.rom",
      "*.data.rom",
    };

    file rom({destination, Location::prefix(packPath), ".sfc"}, file::mode::write);
    for(string& romName : roms) {
      if(auto file = fetch(romName)) {
        rom.write(pack.extract(file()).data(), file().size);
      }
    }
    rom.close();

    break;
  }

  }

  if(usesPatch) {
    switch(patchResult) {
    case bpspatch::result::unknown:
      error("There was an unspecified problem in exporting the MSU-1 pack.");
      break;
    case bpspatch::result::patch_invalid_header:
      error("The BPS patch's header is invalid!");
      break;
    case bpspatch::result::target_too_small:
    case bpspatch::result::target_checksum_invalid:
    case bpspatch::result::patch_too_small:
    case bpspatch::result::patch_checksum_invalid:
      error("The BPS patch is corrupt!");
      break;
    case bpspatch::result::source_too_small:
      error({
        "This ROM is too small!\n",
        "Check that you are selecting the correct ROM, and try again."
      });
      break;
    case bpspatch::result::source_checksum_invalid:
      warning({
        "The ROM's checksum does not match the one in the patch.\n",
        "If you are applying multiple patches to a ROM, this is normal.\n",
        "Otherwise, make sure you select the correct ROM."
      });
      break;
    }
  }

  thread::create([&](uintptr_t) -> void {
    while(zipIndex < pack.file.size()) iterateExport();
    finishExport();
  });
}

auto Program::iterateExport() -> void {
  auto& file = pack.file[zipIndex++];
  exportSettings->setFilename(file.name);

  switch(exportMethod) {

  case ExportMethod::GamePak: {
    if(Location::suffix(file.name) != ".rom" && Location::suffix(file.name) != ".pcm") return;
    file::write({destination, file.name}, pack.extract(file));

    break;
  }

  case ExportMethod::SD2SNES: {
    if(file.name == "msu1.rom") {
      file::write({destination, Location::prefix(packPath), ".msu"}, pack.extract(file));
    } else if(Location::suffix(file.name) == ".pcm") {
      uint trackID;
      if(file.name.beginsWith("track-")) {  //Track name is /track-([0-9]+)\.pcm/, where $1 is the ID
        trackID = string{file.name}.trimLeft("track-").trimRight(".pcm").natural();
      } else {  //Track name is /([0-9]+)([^0-9].*)\.pcm/, where $1 is the ID and $2 is the name
        uint length = 0;
        for(uint pos : range(file.name.size())) {
          if(file.name[pos] < '0' || file.name[pos] > '9') { length = pos; break; }
        }
        trackID = slice(file.name, 0, length).natural();
      }
      file::write({destination, Location::prefix(packPath), "-", trackID, ".pcm"}, pack.extract(file));
    }

    break;
  }

  }

  exportSettings->setProgress(zipIndex, pack.file.size());
}

auto Program::finishExport() -> void {
  if(exportManifest) {
    switch(exportMethod) {

    case ExportMethod::GamePak: {
      if(auto manifest = execute("icarus", "--manifest", destination)) {
        string legacyOutput = "";
        if(auto legacyManifest = execute("daedalus", "--manifest", destination)) {
          legacyOutput = {legacyManifest.output.split("\n\n")[0], "\n\n"};
        }
        file::write({destination, "manifest.bml"}, {legacyOutput, manifest.output});
      }

      break;
    }

    }
  }

  information("MSU-1 pack exported!");
  quit();
}

auto Program::information(const string& text) -> void {
  MessageDialog().setTitle("Mercurial Magic").setText(text).information();
}

auto Program::warning(const string& text) -> void {
  MessageDialog().setTitle("Mercurial Magic").setText(text).warning();
}

auto Program::error(const string& text) -> void {
  MessageDialog().setTitle("Mercurial Magic").setText(text).error();
  quit();
}

auto Program::main() -> void {
  usleep(2000);
}

auto Program::quit() -> void {
  exportSettings->setVisible(false);
  Application::quit();
}

#include <nall/main.hpp>
auto nall::main(string_vector args) -> void {
  Application::setName("Mercurial Magic");
  new Program(args);
  Application::run();
}
