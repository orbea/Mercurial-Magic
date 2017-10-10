auto Daedalus::sufamiTurboManifest(string location) -> string {
  vector<uint8_t> buffer;
  concatenate(buffer, {location, "program.rom"});
  return sufamiTurboManifest(buffer, location);
}

auto Daedalus::sufamiTurboManifest(vector<uint8_t>& buffer, string location) -> string {
  string markup;
  string digest = Hash::SHA256(buffer.data(), buffer.size()).digest();

  if(!markup) {
    SufamiTurboCartridge cartridge{buffer.data(), buffer.size()};
    if(markup = cartridge.markup) {
      markup.append("\n");
      markup.append("information\n");
      markup.append("  title:  ", Location::prefix(location), "\n");
      markup.append("  sha256: ", digest, "\n");
      markup.append("  note:   ", "heuristically generated by daedalus\n");
    }
  }

  return markup;
}

auto Daedalus::sufamiTurboImport(vector<uint8_t>& buffer, string location) -> string {
  auto name = Location::prefix(location);
  auto source = Location::path(location);
  string target{settings["Library/Location"].text(), "Sufami Turbo/", name, ".st/"};

  auto manifest = sufamiTurboManifest(buffer, location);
  if(!manifest) return failure("failed to parse ROM image");

  write({target, "manifest.bml"}, manifest);
  write({target, "program.rom"}, buffer);
  return success(target);
}
