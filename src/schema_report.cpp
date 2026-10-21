#include <spine/schema_report.hpp>

#include <sstream>

namespace spine {
namespace {

void list(std::ostringstream& out, const char* label, const std::vector<std::string>& items) {
  if (items.empty()) return;
  out << "    " << label << " (" << items.size() << "):";
  for (const std::string& item : items) out << "\n      " << item;
  out << "\n";
}

}  // namespace

std::string SchemaReport::summary() const {
  std::ostringstream out;
  out << path << "\n  SPINE " << (spine_version.empty() ? "unknown" : spine_version)
      << ", format_version " << format_version << " (" << layout << "), read with model "
      << model_version << "\n";
  for (const ProductReport& product : products) {
    out << "  " << product.product << " [" << product.class_name << "]: "
        << (product.complete() ? "all model fields read" : "some model fields missing") << "\n";
    list(out, "missing", product.missing);
    list(out, "mismatched", product.mismatched);
    list(out, "not in model", product.extra);
    list(out, "enum labels", product.enum_notes);
  }
  if (!skipped.empty()) {
    out << "  not read:";
    for (const std::string& item : skipped) out << "\n    " << item;
    out << "\n";
  }
  return out.str();
}

}  // namespace spine
