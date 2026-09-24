# AuroraGlass Versioned Public API Baseline

## SDK version

- Canonical SDK version: 0.8.0
- Canonical source of truth: VERSION
- Status: pre-v1 SDK maturity baseline
- Separate native C ABI version: not introduced in Slice A

Managed metadata derives from VERSION through eng/AuroraGlass.Version.props.
Native version metadata derives from VERSION through the generated auroraglass/version.h header.

## Public SDK API

Native public headers: api/native_public_headers.txt
WPF public API: api/wpf_public_api.txt
WinUI 3 public API: api/winui_public_api.txt
WPF interop exports: api/wpf_interop_exports.txt
WinUI interop exports: api/winui_interop_exports.txt

## Internal implementation

Known internal native headers: api/internal_native_headers.txt
Header visibility alone does not define public SDK contract.

## Sample and test boundaries

samples is SAMPLE-ONLY.
tests is TEST-ONLY.

## Compatibility baseline

P8_Version_Tests verifies canonical native version availability.
P8_Public_API_Baseline verifies required native symbols, WPF and WinUI public managed API snapshots, and exact native interop export inventories.

Complete SemVer rules and packaging remain later P8 work.
