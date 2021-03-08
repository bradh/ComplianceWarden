#include <cstring>
#include "spec.h"
#include "fourcc.h"
#include <algorithm> // std::find

bool isVisualSampleEntry(uint32_t fourcc);
void checkEssential(Box const& root, IReport* out, uint32_t fourcc);

static const SpecDesc specHeif =
{
  "heif",
  "HEIF - ISO/IEC 23008-12 - 2nd Edition N18310",
  { "isobmff" },
  {
    {
      "Section 10..2.1.1\n"
      "Files shall contain the brand 'mif1' in the compatible brands array of the\n"
      "FileTypeBox.",
      [] (Box const& root, IReport* out)
      {
        if(root.children.empty() || root.children[0].fourcc != FOURCC("ftyp"))
        {
          out->error("'ftyp' box not found");
          return;
        }

        auto& ftypBox = root.children[0];

        bool found = false;

        for(auto& brand : ftypBox.syms)
          if(!strcmp(brand.name, "compatible_brand") && brand.value == FOURCC("mif1"))
            found = true;

        if(!found)
          out->error("'mif1' brand not found in 'ftyp' box");
      }
    },
    {
      "Section 6.2\n"
      "A MetaBox ('meta'), as specified in ISO/IEC 14496-12, is required at file level.",
      [] (Box const& root, IReport* out)
      {
        bool found = false;

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            found = true;

        if(!found)
          out->error("'meta' box not found at file level");
      }
    },
    {
      "Section 6.2\n"
      "The handler type for the MetaBox shall be 'pict'.",
      [] (Box const& root, IReport* out)
      {
        bool found = false;

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            for(auto& metaChild : box.children)
              if(metaChild.fourcc == FOURCC("hdlr"))
                for(auto& field : metaChild.syms)
                  if(!strcmp(field.name, "handler_type"))
                  {
                    found = true;

                    if(field.value != FOURCC("pict"))
                      out->error("The handler type for the MetaBox shall be 'pict'");
                  }

        if(!found)
          out->error("'hdlr' not found in MetaBox");
      }
    },
    {
      "Section 7.2.1.9\n"
      "ItemInfoBox "
      "Version 0 or 1 of this box is required by ISO/IEC 23008-12",
      [] (Box const& root, IReport* out)
      {
        bool found = false;

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            for(auto& metaChild : box.children)
              if(metaChild.fourcc == FOURCC("iinf"))
                for(auto& field : metaChild.syms)
                  if(!strcmp(field.name, "version"))
                  {
                    if(field.value == 0 || field.value == 1)
                      found = true;
                    else
                      out->error("Version 0 or 1 of ItemInfoBox is required");
                  }

        if(!found)
          out->error("ItemInfoBox is required");
      },
    },
    {
      "Section 6.5.3.1\n"
      "Every image item shall be associated with one [image spatial extents property],\n"
      "prior to the association of all transformative properties.",
      [] (Box const& root, IReport* out)
      {
        std::vector<uint32_t /*4cc*/> properties;

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            for(auto& metaChild : box.children)
              if(metaChild.fourcc == FOURCC("iprp"))
                for(auto& iprpChild : metaChild.children)
                  if(iprpChild.fourcc == FOURCC("ipco"))
                    for(auto& ipcoChild : iprpChild.children)
                      properties.push_back(ipcoChild.fourcc);

        if(std::find(properties.begin(), properties.end(), FOURCC("ispe")) == properties.end())
          out->error("image spatial extents property (\"ispe\") not detected.");

        auto isTransformative = [] (uint32_t fourcc) {
            if(fourcc == FOURCC("clap") || fourcc == FOURCC("irot") || fourcc == FOURCC("imir"))
              return true;
            else
              return false;
          };

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            for(auto& metaChild : box.children)
              if(metaChild.fourcc == FOURCC("iprp"))
                for(auto& iprpChild : metaChild.children)
                  if(iprpChild.fourcc == FOURCC("ipma"))
                  {
                    uint32_t localItemId = 0;
                    bool foundIspe = true;
                    auto checkIspe = [&] () {
                        if(!foundIspe)
                          out->error("Item ID=%u: missing Image spatial extents property", localItemId);
                      };

                    for(auto& sym : iprpChild.syms)
                      if(!strcmp(sym.name, "item_ID"))
                      {
                        checkIspe();
                        foundIspe = false;
                        localItemId = sym.value;
                      }
                      else if(!strcmp(sym.name, "property_index"))
                      {
                        if(sym.value /*1-based*/ > (int)properties.size())
                        {
                          out->error("Invalid property_index=%lld", sym.value);
                        }
                        else
                        {
                          if(properties[sym.value - 1] == FOURCC("ispe"))
                          {
                            foundIspe = true;
                            break;
                          }
                          else if(isTransformative(properties[sym.value - 1]))
                            out->error("Item ID=%u: transformative property \"%s\" (index=%lld) found prior to \"ispe\"",
                                       localItemId, toString(properties[sym.value - 1]).c_str(), sym.value);
                        }
                      }

                    checkIspe();
                  }
      },
    },
    {
      "Section 7.2.3.1\n"
      "The CodingConstraintsBox shall be present in the sample description entry for\n"
      "tracks with handler_type equal to 'pict'",
      [] (Box const& root, IReport* out)
      {
        for(auto& box : root.children)
          if(box.fourcc == FOURCC("moov"))
            for(auto& moovChild : box.children)
              if(moovChild.fourcc == FOURCC("trak"))
                for(auto& trakChild : moovChild.children)
                  if(trakChild.fourcc == FOURCC("mdia"))
                    for(auto& mdiaChild : trakChild.children)
                    {
                      auto isPict = [&] () {
                          if(mdiaChild.fourcc == FOURCC("hdlr"))
                            for(auto& sym : mdiaChild.syms)
                              if(!strcmp(sym.name, "handler_type"))
                                if(sym.value == FOURCC("pict"))
                                  return true;

                          return false;
                        };

                      if(!isPict())
                        continue;

                      if(mdiaChild.fourcc == FOURCC("minf"))
                        for(auto& minfChild : mdiaChild.children)
                          if(minfChild.fourcc == FOURCC("stbl"))
                            for(auto& stblChild : minfChild.children)
                              if(stblChild.fourcc == FOURCC("stsd"))
                                for(auto& stsdChild : stblChild.children)
                                {
                                  if(isVisualSampleEntry(stsdChild.fourcc))
                                  {
                                    bool foundCcst = false;

                                    for(auto& sampleEntryChild : stsdChild.children)
                                      if(sampleEntryChild.fourcc == FOURCC("ccst"))
                                      {
                                        if(!foundCcst)
                                          foundCcst = true;
                                        else
                                          out->error("CodingConstraintsBox ('ccst') is present several times");
                                      }

                                    if(!foundCcst)
                                      out->error("CodingConstraintsBox ('ccst') shall be present once");
                                  }
                                }
                    }
      },
    },
    {
      "Section 6.4.2\n"
      "The primary item shall not be a hidden image item",
      [] (Box const& root, IReport* out)
      {
        uint32_t primaryItemId = -1;

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            for(auto& metaChild : box.children)
              if(metaChild.fourcc == FOURCC("pitm"))
                for(auto& field : metaChild.syms)
                  if(!strcmp(field.name, "item_ID"))
                    primaryItemId = field.value;

        for(auto& box : root.children)
          if(box.fourcc == FOURCC("meta"))
            for(auto& metaChild : box.children)
              if(metaChild.fourcc == FOURCC("iinf"))
                for(auto& iinfChild : metaChild.children)
                  if(iinfChild.fourcc == FOURCC("infe"))
                    for(auto& sym : iinfChild.syms)
                    {
                      if(!strcmp(sym.name, "flags"))
                        if(!(sym.value & 1))
                          // ISOBMFF 8.11.6.1: (flags & 1) equal to 1 indicates that the item is not intended to be a part of the presentation
                          break;

                      if(!strcmp(sym.name, "item_ID"))
                        if(sym.value == primaryItemId)
                          out->error("The primary item shall not be a hidden image item");
                    }
      },
    },
    {
      "Section 6.5.11.1\n"
      "essential shall be equal to 1 for an 'lsel' item property.",
      [] (Box const& root, IReport* out)
      {
        checkEssential(root, out, FOURCC("lsel"));
      }
    },
    {
      "Section B.2.3.1\n"
      "essential shall be equal to 1 for an 'hvcC' item property",
      [] (Box const& root, IReport* out)
      {
        checkEssential(root, out, FOURCC("hvcC"));
      }
    },
    {
      "Section B.2.3.3\n"
      "essential shall be equal to 1 for an 'lhvC' item property",
      [] (Box const& root, IReport* out)
      {
        checkEssential(root, out, FOURCC("lhvC"));
      }
    },
    {
      "Section B.2.3.5.1\n"
      "essential shall be equal to 1 for an 'tols' item property",
      [] (Box const& root, IReport* out)
      {
        checkEssential(root, out, FOURCC("tols"));
      }
    },
    {
      "Section E.2.3\n"
      "essential shall be equal to 1 for an 'avcC' item property",
      [] (Box const& root, IReport* out)
      {
        checkEssential(root, out, FOURCC("avcC"));
      }
    },
    {
      "Section H.2.2\n"
      "essential shall be equal to 1 for an 'jpgC' item property ",
      [] (Box const& root, IReport* out)
      {
        checkEssential(root, out, FOURCC("jpgC"));
      }
    }
  },
  nullptr,
};

static auto const registered = registerSpec(&specHeif);

