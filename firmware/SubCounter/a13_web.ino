// ════════════════════════════════════════════════════════════════════════════
//  Web: dashboard (/), data API, settings (/settings)
// ════════════════════════════════════════════════════════════════════════════
// ── Browser icons: the same "Creators" badge as the home-menu tile ──────────
const char ICON_SVG[] PROGMEM = R"SVG(<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'><defs><clipPath id='c'><rect x='2' y='11' width='60' height='42' rx='12'/></clipPath></defs><g clip-path='url(#c)'><rect x='2' y='11' width='30' height='42' fill='#ff0033'/><rect x='32' y='11' width='30' height='42' fill='#9146ff'/></g><path d='M15 23v18l13-9z' fill='#fff'/><path d='M40 24h4v11h-4zM48 24h4v11h-4z' fill='#fff'/></svg>)SVG";
const uint8_t ICON_PNG32[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x03, 0xc9, 0x49, 0x44, 0x41,
  0x54, 0x78, 0xda, 0xed, 0x97, 0x4d, 0x68, 0x5c, 0x55, 0x14, 0xc7, 0x7f, 0xe7, 0xbc, 0xf7, 0x26, 0xc9, 0x38, 0x93, 0x44,
  0x5b, 0x44, 0x0a, 0xd2, 0x95, 0x88, 0xa9, 0x0a, 0x1a, 0x10, 0x8b, 0x4a, 0x29, 0xe8, 0x46, 0xba, 0x9d, 0x2e, 0x6c, 0x45,
  0x5d, 0x88, 0xdd, 0x25, 0x2a, 0x14, 0xd4, 0x45, 0x9a, 0x22, 0x28, 0x82, 0x5a, 0x10, 0xb5, 0xc5, 0x8f, 0x42, 0x69, 0x11,
  0x32, 0x82, 0x2e, 0x54, 0x62, 0xb1, 0x98, 0x82, 0x60, 0x1b, 0x2c, 0x0d, 0x74, 0x11, 0x84, 0x82, 0x0b, 0x37, 0xa2, 0x16,
  0xc9, 0x87, 0x49, 0x66, 0xe6, 0xbd, 0x7b, 0x5c, 0xdc, 0x37, 0xc9, 0xcc, 0x74, 0xde, 0xf4, 0xa5, 0x22, 0x75, 0xd1, 0x0b,
  0x87, 0x77, 0xcf, 0x7b, 0xf7, 0xbc, 0xf3, 0x3f, 0x1f, 0xf7, 0xdc, 0x73, 0xe1, 0xe6, 0xb8, 0xc1, 0x43, 0x3a, 0x5f, 0x18,
  0x08, 0x54, 0x14, 0x7e, 0x97, 0x3c, 0x3f, 0x98, 0xd9, 0x35, 0xc3, 0x4c, 0x8e, 0x75, 0x3b, 0x6e, 0xc7, 0x2a, 0x55, 0x9c,
  0x20, 0x96, 0xb9, 0xc8, 0xa8, 0x04, 0xff, 0xb5, 0xc5, 0x53, 0x15, 0x0b, 0xba, 0x7a, 0xc0, 0x40, 0x05, 0x9c, 0x31, 0x52,
  0x82, 0xe2, 0x6e, 0xb0, 0x11, 0x90, 0x7e, 0x70, 0x80, 0xb4, 0x79, 0xc3, 0x01, 0x4a, 0xcc, 0x5a, 0x74, 0x1b, 0x27, 0x1e,
  0xfd, 0x92, 0x95, 0xc2, 0x20, 0xea, 0xfc, 0xdb, 0x36, 0x83, 0x04, 0x53, 0x40, 0x95, 0x5a, 0xa0, 0xcc, 0xd7, 0x1d, 0xdf,
  0x8f, 0x4d, 0xcb, 0xe2, 0xc4, 0x84, 0xe9, 0xe4, 0xa4, 0xb8, 0x75, 0x00, 0x46, 0x25, 0x10, 0xaa, 0x89, 0xf1, 0xc0, 0x7e,
  0x08, 0x5e, 0x07, 0xdd, 0xde, 0x25, 0x3a, 0x1d, 0xb8, 0xeb, 0x10, 0x6d, 0xe5, 0xd4, 0xee, 0x6f, 0xf9, 0xbb, 0x00, 0xa1,
  0x81, 0x65, 0x7a, 0x16, 0x9c, 0x41, 0xe2, 0xf8, 0xb5, 0xee, 0x98, 0x18, 0x3f, 0x2d, 0xc7, 0xa7, 0x2a, 0x16, 0xec, 0xad,
  0x4a, 0x12, 0x6e, 0x28, 0x1f, 0x7d, 0x09, 0xc2, 0xb7, 0x21, 0x01, 0xe2, 0x84, 0xec, 0xff, 0xe1, 0x10, 0x94, 0x98, 0x55,
  0x1a, 0x2c, 0x37, 0x16, 0x58, 0x95, 0x21, 0x02, 0x73, 0x58, 0x87, 0x07, 0xda, 0x40, 0x18, 0x12, 0x06, 0xdc, 0x59, 0x2e,
  0xf0, 0xe9, 0xbb, 0x4f, 0xd8, 0x1d, 0x7b, 0xab, 0xf2, 0xc6, 0x54, 0xc5, 0x82, 0xd4, 0x03, 0xa3, 0x0f, 0x82, 0x5e, 0x00,
  0x97, 0x80, 0x09, 0x88, 0xf6, 0x8a, 0xa3, 0x21, 0x08, 0x8d, 0xb6, 0x10, 0x04, 0x66, 0x18, 0xd7, 0xc8, 0x5b, 0xc3, 0x89,
  0xe2, 0x0a, 0x4a, 0x58, 0x33, 0x76, 0x8e, 0x4d, 0xcb, 0xb9, 0xa6, 0xa2, 0x17, 0x7d, 0xfc, 0xcc, 0xda, 0x94, 0xab, 0x74,
  0x86, 0x3f, 0xdf, 0xbe, 0x92, 0x0c, 0x5e, 0x50, 0x33, 0x08, 0x14, 0x33, 0xc7, 0xcb, 0x00, 0x6a, 0x8c, 0x14, 0x80, 0x9d,
  0x69, 0xb2, 0x69, 0x9b, 0xa4, 0xab, 0x81, 0x25, 0xa0, 0xf9, 0x37, 0x87, 0x8b, 0x3d, 0x65, 0xf1, 0x06, 0x41, 0x3d, 0x41,
  0x04, 0x1e, 0x3a, 0xb6, 0xc7, 0x8a, 0x0a, 0xb7, 0x94, 0x81, 0xc1, 0x34, 0xe4, 0x1e, 0xab, 0x08, 0xd0, 0x80, 0x7b, 0xef,
  0x81, 0x62, 0x11, 0xdc, 0x22, 0x84, 0x61, 0x2e, 0xeb, 0x07, 0x86, 0x3c, 0x35, 0x2d, 0x6f, 0xf2, 0xad, 0x8e, 0x34, 0x03,
  0x33, 0xca, 0x0d, 0xc7, 0x60, 0xf7, 0x58, 0x07, 0x0a, 0x2c, 0xc1, 0x33, 0x7b, 0xe0, 0xc7, 0xe3, 0xb0, 0xe3, 0x6e, 0x88,
  0xaf, 0x80, 0xaa, 0xa7, 0x4e, 0xbd, 0xe2, 0xad, 0xec, 0x2b, 0xc2, 0x53, 0x1f, 0xc2, 0xd3, 0x1f, 0xfb, 0x79, 0x54, 0x80,
  0x7d, 0x47, 0x61, 0xff, 0x47, 0xd0, 0x57, 0xf6, 0x6b, 0x3a, 0x23, 0xda, 0x23, 0xd9, 0x14, 0x16, 0x96, 0xe1, 0xfe, 0xbb,
  0xe0, 0xdc, 0x67, 0x30, 0xf6, 0x3c, 0xb8, 0x15, 0x70, 0x6b, 0xd9, 0xde, 0xc8, 0xe9, 0x81, 0x9c, 0x00, 0x80, 0x30, 0x00,
  0xe7, 0x60, 0xa0, 0x00, 0x47, 0x0e, 0xc2, 0xd7, 0xc7, 0x60, 0xfb, 0x36, 0x88, 0xff, 0xcc, 0x14, 0xbd, 0x56, 0x0e, 0x6c,
  0x0e, 0x00, 0xbe, 0x8c, 0xe1, 0xcc, 0x97, 0x86, 0x27, 0x1f, 0x81, 0xb9, 0xcf, 0xe1, 0xc0, 0x73, 0xa0, 0xf5, 0xae, 0x66,
  0x39, 0xe7, 0x29, 0x8b, 0xbf, 0xca, 0xc6, 0x4d, 0x17, 0xf3, 0x28, 0x84, 0x42, 0xd4, 0xb5, 0x4e, 0x89, 0x40, 0x7f, 0x79,
  0x63, 0x8e, 0x40, 0x5f, 0xa9, 0x85, 0xbf, 0x2e, 0x00, 0xce, 0x79, 0xe9, 0x30, 0x80, 0x33, 0xb3, 0x70, 0xe0, 0x30, 0x5c,
  0xfe, 0x19, 0x28, 0xfb, 0x74, 0x6e, 0x2a, 0x57, 0x88, 0x6b, 0x70, 0xfe, 0xa4, 0xdf, 0xb5, 0x71, 0x0d, 0xcc, 0xc1, 0xec,
  0x29, 0x0f, 0xa4, 0xb1, 0xd6, 0xbd, 0xbc, 0xf5, 0x06, 0x10, 0x27, 0x3e, 0x04, 0xb5, 0x3a, 0xbc, 0xf6, 0x1e, 0xbc, 0xf5,
  0x49, 0x2a, 0xb5, 0x15, 0xe2, 0xd5, 0xb6, 0x6d, 0xa5, 0x0a, 0x8d, 0x1a, 0x9c, 0xfd, 0xc0, 0xbf, 0xeb, 0x2b, 0xf9, 0x3c,
  0x9c, 0x79, 0xbf, 0x85, 0xd7, 0x14, 0xb3, 0xe4, 0x02, 0xe0, 0x60, 0xb8, 0x0c, 0xf3, 0xbf, 0xc0, 0xbe, 0x83, 0x70, 0x71,
  0x0e, 0x74, 0xd8, 0x4b, 0xc7, 0xdd, 0xb3, 0x4a, 0x04, 0x8a, 0xc3, 0xa9, 0x74, 0xe2, 0x9f, 0x9d, 0x7c, 0xbe, 0x24, 0x4c,
  0x1c, 0x50, 0x82, 0x93, 0xdf, 0xc0, 0xc3, 0xcf, 0xc2, 0xc5, 0x4b, 0x10, 0x6e, 0xf1, 0xc9, 0xd8, 0x2b, 0xa3, 0x52, 0x45,
  0xad, 0xca, 0x3a, 0xf9, 0x2e, 0x00, 0xfe, 0x5a, 0x06, 0x5b, 0x5a, 0x3f, 0x99, 0x9b, 0x3e, 0xa5, 0x00, 0x73, 0x97, 0x60,
  0x71, 0x09, 0x74, 0x28, 0xd3, 0xea, 0xeb, 0x6a, 0xc3, 0x7c, 0x08, 0x96, 0x23, 0x65, 0x51, 0x85, 0xcb, 0x35, 0xe0, 0x3c,
  0xa8, 0xf9, 0xb4, 0x69, 0x6d, 0x51, 0xfa, 0x41, 0x82, 0xde, 0x26, 0x6c, 0xbe, 0x07, 0x4c, 0x22, 0xc5, 0x44, 0xf8, 0xe9,
  0x85, 0xaf, 0x64, 0x25, 0x0d, 0x81, 0x1c, 0x01, 0x27, 0xa9, 0x17, 0x36, 0x40, 0x38, 0x6b, 0xcb, 0xf4, 0x7f, 0x3d, 0x0c,
  0x27, 0x69, 0xdf, 0x69, 0xca, 0x3b, 0xe9, 0x69, 0x58, 0x09, 0x84, 0x0b, 0xb3, 0xe0, 0x5e, 0xf1, 0x35, 0x56, 0xd4, 0x1f,
  0x81, 0x16, 0x67, 0x91, 0xad, 0x3f, 0x49, 0xe7, 0xa4, 0xdf, 0xe8, 0x49, 0x81, 0xa2, 0xa5, 0x02, 0xe1, 0x6a, 0xcc, 0xa1,
  0xf1, 0x69, 0xf9, 0x61, 0xaa, 0x62, 0x41, 0xe8, 0xbb, 0xa1, 0x4a, 0x20, 0x54, 0xdf, 0x34, 0x46, 0xff, 0x00, 0x39, 0x0c,
  0xe1, 0xb6, 0x5e, 0x2d, 0x99, 0xe2, 0xed, 0x18, 0x20, 0xa2, 0x14, 0x0d, 0x21, 0x11, 0x84, 0xa6, 0x5d, 0x5b, 0x28, 0x6b,
  0xd9, 0xaa, 0xb1, 0xe3, 0xb7, 0xc5, 0x06, 0x93, 0xe3, 0xa7, 0xe5, 0x68, 0xb3, 0x25, 0xeb, 0xd2, 0x94, 0xde, 0x77, 0x2b,
  0x0c, 0x3c, 0x0e, 0x6e, 0x04, 0xac, 0x3f, 0xbb, 0xeb, 0x68, 0xb0, 0x16, 0x6d, 0xe1, 0xc4, 0x63, 0x5f, 0xb0, 0x12, 0x0d,
  0x12, 0xba, 0xd6, 0xd8, 0x5d, 0x55, 0xcd, 0x6b, 0xaa, 0xcc, 0x2f, 0xd4, 0xf9, 0xee, 0xd5, 0x33, 0x72, 0xa5, 0xb5, 0x29,
  0xfd, 0xff, 0xb4, 0xe5, 0xed, 0x17, 0x93, 0x5d, 0xb9, 0x81, 0xe4, 0xbd, 0x98, 0x00, 0x1c, 0x3a, 0x4b, 0xd2, 0xf3, 0x62,
  0x72, 0x73, 0xdc, 0x88, 0xf1, 0x0f, 0xb2, 0xb9, 0x9d, 0xe7, 0x92, 0xb4, 0xed, 0xb1, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
  0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
const uint8_t ICON_PNG180[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0xb4,
  0x00, 0x00, 0x00, 0xb4, 0x08, 0x02, 0x00, 0x00, 0x00, 0xb2, 0xaf, 0x91, 0x65, 0x00, 0x00, 0x0f, 0x10, 0x49, 0x44, 0x41,
  0x54, 0x78, 0xda, 0xed, 0x9d, 0x79, 0x90, 0x5c, 0xd5, 0x75, 0x87, 0x7f, 0xe7, 0xde, 0xfb, 0x5e, 0x77, 0x4f, 0xcf, 0x3e,
  0x08, 0x3c, 0x48, 0x16, 0x60, 0x14, 0x10, 0x62, 0xb1, 0x71, 0x9c, 0x14, 0x62, 0x09, 0x8e, 0x13, 0x82, 0xb1, 0x62, 0x42,
  0xa5, 0xe2, 0x0a, 0x84, 0x10, 0x27, 0x01, 0x8b, 0x08, 0xaa, 0x52, 0x66, 0x31, 0x26, 0x65, 0xb2, 0x20, 0x6c, 0xc5, 0x78,
  0xc1, 0x0e, 0x76, 0x51, 0x80, 0xd8, 0x0a, 0x19, 0x10, 0x81, 0x00, 0x29, 0x2c, 0x12, 0x0b, 0x93, 0x80, 0x65, 0xc4, 0x66,
  0x40, 0xd8, 0xb1, 0x31, 0x04, 0x81, 0xf6, 0x19, 0x21, 0x69, 0x16, 0xcd, 0x4c, 0xaf, 0xf7, 0x9e, 0x93, 0x3f, 0x7a, 0x46,
  0x5b, 0x46, 0xc6, 0xd5, 0xeb, 0x13, 0x3a, 0x5f, 0xf5, 0x5f, 0xa3, 0xb9, 0x9a, 0xee, 0xf7, 0xbe, 0x3e, 0xf7, 0x9c, 0xf3,
  0xee, 0x7d, 0x8f, 0x3a, 0x3a, 0xba, 0xa0, 0x28, 0xd3, 0x61, 0xf4, 0x10, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x2a, 0x87,
  0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x8a, 0xca,
  0xa1, 0xa8, 0x1c, 0x8a, 0xca, 0xa1, 0xa8, 0x1c, 0x8a, 0xca, 0xa1, 0xa8, 0x1c, 0x8a, 0xca, 0xa1, 0xa8, 0x1c, 0x8a, 0xca,
  0xa1, 0xa8, 0x1c, 0x8a, 0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2,
  0x72, 0x28, 0x2a, 0x87, 0xa2, 0x72, 0x28, 0x2a, 0x87, 0xa2, 0xa8, 0x1c, 0x8a, 0xca, 0xa1, 0xa8, 0x1c, 0x8a, 0xca, 0xa1,
  0xa8, 0x1c, 0x8a, 0xca, 0xa1, 0x24, 0x13, 0xd7, 0xc2, 0xbf, 0x4d, 0xbb, 0x5f, 0x94, 0x90, 0xc3, 0xc1, 0x64, 0x13, 0x73,
  0x6a, 0x44, 0x20, 0x10, 0x11, 0xc8, 0x41, 0x24, 0x87, 0x01, 0x0c, 0x48, 0x20, 0x25, 0xa0, 0x04, 0xf1, 0x24, 0x68, 0xdd,
  0xe7, 0xdf, 0xdb, 0x55, 0xb6, 0xe5, 0x91, 0x24, 0x78, 0x2a, 0x80, 0x01, 0x19, 0xe3, 0x1c, 0xa5, 0xac, 0x89, 0x08, 0x46,
  0xc0, 0x22, 0xfc, 0xbe, 0x95, 0x83, 0xa6, 0x9c, 0xc8, 0x41, 0xca, 0x14, 0x08, 0x34, 0x43, 0xec, 0x11, 0x1c, 0x1f, 0xc5,
  0xd1, 0x2c, 0x89, 0x0e, 0x63, 0xdb, 0x27, 0x56, 0x20, 0xd4, 0x32, 0x33, 0x82, 0xb7, 0x6d, 0x2f, 0xcc, 0x59, 0x54, 0x76,
  0x19, 0x12, 0x46, 0x2b, 0x82, 0x99, 0x40, 0x0c, 0x51, 0xde, 0x8f, 0xee, 0x2c, 0x0d, 0x8e, 0x96, 0x36, 0x0f, 0x15, 0xd6,
  0x0d, 0x17, 0xd6, 0xef, 0x2c, 0x0f, 0x7a, 0xf6, 0x91, 0x89, 0x23, 0xd3, 0x66, 0xc8, 0x8a, 0x84, 0xa6, 0xc5, 0x12, 0xd7,
  0x9c, 0x03, 0x6f, 0x41, 0x25, 0xc8, 0x4e, 0x0a, 0x91, 0xd0, 0x3c, 0x8e, 0xcf, 0x0a, 0xd9, 0x33, 0x7d, 0xe6, 0xe4, 0x90,
  0x3a, 0x5c, 0x1c, 0xc1, 0x4c, 0x9d, 0x09, 0x41, 0xcb, 0xe6, 0x17, 0x02, 0xca, 0x90, 0x9e, 0xb6, 0xfe, 0xcb, 0x72, 0x31,
  0x8c, 0x24, 0x20, 0x88, 0x09, 0x4a, 0x8c, 0xf1, 0xf2, 0xbb, 0x03, 0x13, 0x3f, 0x7b, 0x67, 0xe7, 0xaa, 0x5f, 0x0e, 0xad,
  0xdc, 0x3c, 0xf1, 0x5a, 0xde, 0xe7, 0x52, 0x36, 0xeb, 0x4c, 0xba, 0x39, 0x8a, 0x50, 0xa3, 0xef, 0x60, 0x6c, 0x01, 0x0f,
  0x8c, 0x53, 0xe8, 0x13, 0xf7, 0x69, 0xdf, 0xfe, 0x17, 0xa5, 0xce, 0xd3, 0x42, 0x26, 0x9e, 0x4c, 0x84, 0x05, 0x10, 0x06,
  0xb8, 0xf5, 0xd3, 0x0a, 0x01, 0xbe, 0x18, 0x75, 0xdf, 0x7b, 0xda, 0x23, 0xf9, 0xb8, 0xd3, 0x48, 0x0b, 0x35, 0xdd, 0xfd,
  0x96, 0x0c, 0x59, 0x4b, 0x70, 0x06, 0x86, 0x50, 0x0c, 0xb2, 0x69, 0xfc, 0xe5, 0x97, 0xdf, 0xfd, 0xde, 0x6b, 0xdb, 0x1e,
  0x19, 0x2a, 0x6e, 0x4c, 0xdb, 0x76, 0x67, 0x52, 0x2c, 0xfe, 0x40, 0x95, 0x83, 0x00, 0x03, 0x8c, 0x10, 0x77, 0x89, 0xb9,
  0xb8, 0xdc, 0x7d, 0x79, 0xa9, 0xfb, 0x43, 0x1c, 0x57, 0xd2, 0xbe, 0x00, 0x01, 0xc8, 0x4c, 0xfd, 0x5a, 0x32, 0x12, 0x0e,
  0x5f, 0x8c, 0xba, 0xef, 0x39, 0xfd, 0xdf, 0x13, 0x23, 0xc7, 0xe4, 0x44, 0x23, 0xc2, 0x80, 0x18, 0x72, 0xb1, 0x81, 0x35,
  0x18, 0x2a, 0x6c, 0x7d, 0x61, 0xf0, 0x8e, 0x1f, 0x6d, 0xb9, 0x65, 0xb4, 0xb8, 0xa5, 0x2d, 0xea, 0x85, 0x88, 0xa0, 0x51,
  0xb9, 0x88, 0x4d, 0xa5, 0xd2, 0x8d, 0x0b, 0x18, 0x63, 0x14, 0xce, 0x2b, 0x77, 0x7c, 0x2f, 0xdf, 0x7f, 0x51, 0xb9, 0xbb,
  0x47, 0x4c, 0x00, 0xcb, 0x54, 0xf2, 0x61, 0xa6, 0x4a, 0x15, 0x24, 0x45, 0x0e, 0x0e, 0x36, 0xbd, 0x66, 0xf6, 0x05, 0xde,
  0xa6, 0x28, 0x31, 0xd2, 0x12, 0x88, 0xc8, 0x10, 0x19, 0x00, 0x9e, 0xb9, 0xc4, 0x9c, 0x76, 0x1d, 0xc7, 0xf5, 0xfe, 0xce,
  0x49, 0x87, 0x7c, 0xa6, 0x14, 0x26, 0xd6, 0xed, 0x7c, 0x01, 0x04, 0x67, 0x52, 0x0d, 0xf2, 0xa3, 0x21, 0x7d, 0x0e, 0x07,
  0x1a, 0x23, 0x8e, 0x41, 0x4b, 0xf3, 0xfd, 0x8f, 0xe6, 0x67, 0x9d, 0xc0, 0x29, 0x0f, 0xcf, 0x10, 0x0b, 0xb2, 0x09, 0x12,
  0xe2, 0x00, 0x83, 0xc8, 0x18, 0xb2, 0x41, 0x64, 0xac, 0xec, 0xbb, 0xe2, 0xd9, 0x7f, 0x3e, 0xf7, 0xf6, 0x4b, 0x8e, 0xff,
  0xb7, 0xf6, 0xe8, 0xd0, 0x9c, 0x1f, 0x36, 0xe4, 0x0e, 0x0c, 0x39, 0x22, 0xd0, 0x30, 0x85, 0x93, 0x42, 0xea, 0xe9, 0xdc,
  0x07, 0x2f, 0x29, 0xf7, 0x30, 0x98, 0x21, 0x6e, 0x6a, 0x12, 0x51, 0x6a, 0x8f, 0x25, 0x96, 0x9c, 0x17, 0x9e, 0x28, 0x87,
  0x8f, 0xcc, 0x38, 0xf7, 0x8a, 0x93, 0x57, 0xcd, 0xed, 0x39, 0x6b, 0xa2, 0xbc, 0xa3, 0x11, 0x7e, 0x98, 0xba, 0x9b, 0x31,
  0x44, 0xfe, 0x1c, 0x9f, 0x7d, 0x3a, 0x77, 0xc4, 0x49, 0x21, 0xed, 0xe1, 0x8d, 0x76, 0x61, 0x1b, 0xa2, 0x88, 0x31, 0x64,
  0x27, 0xca, 0xbe, 0x23, 0x9e, 0xb5, 0xe8, 0xc4, 0x27, 0x4e, 0xed, 0xbf, 0x74, 0xac, 0x54, 0x7f, 0x3f, 0xea, 0x79, 0xe2,
  0x1c, 0x68, 0x88, 0xfc, 0x67, 0xcb, 0x5d, 0x8f, 0xe4, 0x66, 0x75, 0x0a, 0x05, 0x04, 0xa7, 0x73, 0x48, 0x23, 0x31, 0xe4,
  0xca, 0x81, 0x3d, 0xd3, 0x05, 0xc7, 0xde, 0x7a, 0xf6, 0x11, 0xd7, 0xe5, 0xca, 0x3b, 0x4c, 0x5d, 0x3b, 0xbc, 0xa6, 0x8e,
  0x66, 0x0c, 0x53, 0x58, 0xe0, 0xdb, 0xef, 0xca, 0xf7, 0xa7, 0x81, 0x4a, 0x86, 0xa1, 0xe7, 0xaf, 0xf1, 0x7e, 0x18, 0x06,
  0x4a, 0x81, 0xff, 0xf8, 0xe8, 0x1b, 0x4e, 0x3b, 0xfc, 0xf2, 0x89, 0xf2, 0x50, 0x1d, 0xe3, 0x47, 0x7d, 0xe4, 0xb0, 0xc0,
  0x28, 0x85, 0xf9, 0x21, 0xf3, 0x50, 0x6e, 0x26, 0x41, 0x18, 0xa2, 0x53, 0x49, 0x33, 0xb3, 0x10, 0x01, 0x4d, 0x78, 0x7f,
  0xfe, 0x31, 0xdf, 0x3d, 0xf9, 0xd0, 0x0b, 0xea, 0x98, 0x7f, 0x98, 0xba, 0xfc, 0x17, 0x25, 0xc8, 0x0c, 0xb6, 0xf7, 0xe5,
  0xfb, 0x33, 0x20, 0xd6, 0x24, 0xa3, 0x15, 0x7e, 0x00, 0xa6, 0xcc, 0x72, 0xc1, 0x31, 0xb7, 0xce, 0x6c, 0x3f, 0xa9, 0x18,
  0xc6, 0xa9, 0x1e, 0xf3, 0x4b, 0x7d, 0xce, 0x63, 0x9e, 0xe4, 0xd6, 0xc2, 0x61, 0x47, 0x71, 0x2a, 0x80, 0xad, 0x9e, 0xab,
  0x16, 0xa5, 0xa8, 0x9e, 0xb9, 0xcd, 0x75, 0x5e, 0x70, 0xcc, 0xdd, 0x8e, 0x22, 0x91, 0x50, 0x7b, 0xd3, 0xa0, 0x56, 0x39,
  0x1c, 0x68, 0x94, 0xf8, 0xe2, 0x52, 0xd7, 0x79, 0xbe, 0xcb, 0x23, 0x68, 0x9e, 0xd1, 0xd2, 0xfc, 0xc3, 0xe6, 0xbc, 0x9f,
  0xd3, 0xf5, 0xd1, 0xdf, 0x9f, 0x7d, 0x5d, 0xde, 0x8f, 0xd6, 0x9e, 0x9c, 0x9a, 0xda, 0x6c, 0x45, 0x11, 0x32, 0x93, 0xdd,
  0x92, 0xe2, 0x21, 0x02, 0xd6, 0xd9, 0xa4, 0xe5, 0x58, 0xb2, 0x13, 0x9e, 0x3f, 0x31, 0xeb, 0xca, 0xd9, 0x1d, 0x1f, 0x2b,
  0x86, 0x31, 0xaa, 0xad, 0xbb, 0x54, 0xd3, 0x60, 0x0b, 0xca, 0x51, 0xf8, 0x62, 0xa9, 0x77, 0x86, 0xc4, 0x41, 0xe5, 0x48,
  0xc6, 0xf4, 0xc2, 0x22, 0x29, 0x1b, 0x9d, 0x7d, 0xc4, 0xf5, 0x2c, 0xa1, 0xc6, 0x55, 0x54, 0xa6, 0x96, 0x91, 0x39, 0xf0,
  0x31, 0x9c, 0xfa, 0xab, 0x72, 0x97, 0xe8, 0x84, 0x92, 0xa4, 0xc9, 0x25, 0x1f, 0xf8, 0x84, 0xbe, 0x73, 0xe6, 0x74, 0x7d,
  0xbc, 0xe0, 0x6b, 0x0a, 0x1e, 0xb5, 0x8c, 0xa4, 0x22, 0xf1, 0x5f, 0x96, 0x3b, 0xdb, 0xc5, 0x85, 0x96, 0x2d, 0xd2, 0x51,
  0xa6, 0x41, 0x84, 0x23, 0x43, 0xf3, 0xfb, 0x17, 0x06, 0xf1, 0xb5, 0x04, 0x8f, 0x2a, 0xe5, 0x20, 0xa0, 0x04, 0xe9, 0x13,
  0x77, 0x61, 0xa9, 0x13, 0x60, 0xa3, 0x61, 0x23, 0x61, 0xc1, 0xa3, 0x10, 0xe4, 0xf8, 0xbe, 0x05, 0x1f, 0x68, 0x3b, 0xae,
  0x14, 0xf2, 0x54, 0xed, 0x59, 0xae, 0x7e, 0x58, 0x8e, 0xf8, 0x4c, 0x9f, 0x99, 0x2d, 0x31, 0x6b, 0xb6, 0x91, 0xbc, 0xcc,
  0x23, 0x70, 0xe8, 0x88, 0xb2, 0xf3, 0xfa, 0xfe, 0xb0, 0xc4, 0xf9, 0xaa, 0x67, 0x96, 0xaa, 0x23, 0x07, 0x09, 0x70, 0xae,
  0x6f, 0x17, 0x80, 0xf5, 0x5c, 0x24, 0xb1, 0xed, 0x41, 0x41, 0xe4, 0xf8, 0xde, 0x05, 0x91, 0x89, 0xab, 0x5e, 0x99, 0x5c,
  0xa5, 0x1c, 0x65, 0x48, 0x97, 0x98, 0x33, 0x42, 0x86, 0x20, 0x3a, 0xa7, 0x24, 0x51, 0x0e, 0x32, 0xa5, 0x40, 0x33, 0xdb,
  0x3f, 0xd6, 0x9b, 0x3e, 0xd2, 0x4b, 0xa1, 0xba, 0xcc, 0xc3, 0x54, 0x37, 0xa6, 0x00, 0x99, 0xcb, 0xf1, 0x91, 0x1c, 0x89,
  0x5e, 0x46, 0x49, 0x6e, 0x4d, 0xcb, 0xed, 0x51, 0xdb, 0xcc, 0xec, 0xc9, 0x65, 0x2e, 0x54, 0x37, 0xb3, 0x54, 0x37, 0x86,
  0x3c, 0xf1, 0x89, 0x9c, 0x32, 0xb0, 0x9c, 0x88, 0x2d, 0x27, 0xca, 0x74, 0x35, 0x0b, 0xd8, 0x10, 0x66, 0xb6, 0x7f, 0x58,
  0xaa, 0x5d, 0x12, 0x5b, 0xfd, 0xd7, 0x7e, 0x0e, 0xc7, 0x80, 0xaa, 0x91, 0xec, 0xc4, 0x43, 0x30, 0x23, 0xf3, 0x1b, 0x44,
  0x54, 0xdd, 0x3e, 0x06, 0x53, 0x95, 0x92, 0x02, 0xd0, 0x51, 0x1c, 0x01, 0x42, 0x9a, 0x70, 0x24, 0x37, 0xed, 0xa0, 0x20,
  0xe8, 0x49, 0x1d, 0x19, 0x99, 0x34, 0xaa, 0xca, 0x49, 0xab, 0x93, 0x03, 0x06, 0x38, 0xa4, 0x71, 0xd7, 0x5f, 0x09, 0x20,
  0x75, 0xae, 0x0e, 0x30, 0x90, 0x71, 0xbd, 0x91, 0xc9, 0x30, 0x9a, 0x25, 0x07, 0x03, 0x19, 0x31, 0xfd, 0x62, 0xd1, 0xa0,
  0xc6, 0xa8, 0x30, 0x84, 0x61, 0x34, 0xd3, 0xad, 0x3d, 0x27, 0x45, 0x5b, 0xd4, 0x97, 0xb2, 0x9d, 0x52, 0xd5, 0x75, 0x16,
  0x93, 0x44, 0xdd, 0x3b, 0xb2, 0x88, 0x2c, 0x78, 0x1c, 0xce, 0xe9, 0x19, 0x6e, 0x21, 0x09, 0x93, 0xc3, 0x18, 0xa0, 0x88,
  0x63, 0x67, 0xe3, 0x47, 0x4b, 0xf1, 0x9b, 0x27, 0xc2, 0x6f, 0x07, 0xd1, 0x81, 0x11, 0x42, 0x64, 0xff, 0xaf, 0x06, 0x0d,
  0x3c, 0xe8, 0xe4, 0xa8, 0x64, 0x1c, 0x85, 0x12, 0x4e, 0xf9, 0x30, 0x7e, 0xbc, 0x0c, 0x57, 0x2c, 0x84, 0x14, 0xc0, 0x85,
  0xe4, 0x87, 0x10, 0x32, 0xfb, 0x79, 0xbd, 0xd7, 0x2e, 0x2e, 0xb2, 0x55, 0x0e, 0x6c, 0x02, 0x89, 0x3c, 0xe8, 0x86, 0x10,
  0x18, 0xe9, 0x18, 0x37, 0x5d, 0x83, 0x73, 0xce, 0xc0, 0xa2, 0xc5, 0x58, 0xbb, 0x16, 0xb6, 0x07, 0xcc, 0x90, 0x64, 0xd5,
  0xce, 0x44, 0xf0, 0x25, 0x74, 0xf5, 0xe3, 0x8f, 0x96, 0xc0, 0xd8, 0x7d, 0xef, 0x13, 0xc0, 0x01, 0xd6, 0x61, 0xcd, 0x23,
  0x78, 0x69, 0x39, 0x32, 0x5d, 0xe0, 0xb0, 0x97, 0x4c, 0xe5, 0x1c, 0x3e, 0x30, 0x0f, 0x9f, 0xba, 0x0e, 0xcc, 0xfb, 0xe6,
  0xdf, 0x95, 0x81, 0xcf, 0xde, 0x85, 0xd7, 0x57, 0x22, 0xdd, 0xb1, 0xd7, 0xc0, 0x83, 0x5e, 0x0e, 0x00, 0xd6, 0x40, 0x04,
  0x81, 0x71, 0xd6, 0x7c, 0xbc, 0xb8, 0x1c, 0x5f, 0xfc, 0x26, 0xee, 0x78, 0x10, 0xc8, 0xc0, 0xa6, 0x10, 0x42, 0xb2, 0xde,
  0xaa, 0xc0, 0x44, 0xe8, 0x3b, 0x62, 0xbf, 0xff, 0xde, 0xd6, 0x33, 0x7d, 0x21, 0x29, 0x82, 0x28, 0x85, 0xee, 0x99, 0xfb,
  0x1d, 0x98, 0x6a, 0x87, 0xb4, 0xf4, 0xc2, 0x55, 0x82, 0xa7, 0x73, 0x22, 0x38, 0x8b, 0xc0, 0xe8, 0xed, 0xc2, 0xd2, 0xc5,
  0x58, 0xfe, 0x2f, 0xe8, 0xef, 0x45, 0x18, 0x81, 0xb5, 0x89, 0x2b, 0x74, 0x05, 0xbe, 0x0c, 0x11, 0x08, 0x43, 0x64, 0xf7,
  0x2b, 0x78, 0x88, 0x80, 0xf7, 0x7f, 0x9f, 0x84, 0xca, 0xaf, 0xf1, 0xde, 0xa3, 0x76, 0x0d, 0x94, 0x56, 0x7f, 0x0b, 0x12,
  0x9f, 0xeb, 0xed, 0x0a, 0x21, 0x7f, 0xfa, 0x49, 0xbc, 0xf0, 0x00, 0xce, 0x3b, 0x1b, 0x61, 0x04, 0x12, 0x60, 0x6d, 0xd2,
  0x4c, 0xde, 0xdf, 0xeb, 0x3d, 0x72, 0x8e, 0x6a, 0x07, 0xaa, 0x1c, 0x53, 0xc7, 0xcf, 0x1a, 0xf8, 0x80, 0x0f, 0x1e, 0x86,
  0x47, 0x6f, 0xc6, 0x6d, 0x5f, 0x46, 0x47, 0x84, 0x30, 0x06, 0xe7, 0xb4, 0x57, 0x76, 0xd0, 0xcb, 0x31, 0x99, 0x1d, 0x59,
  0xb0, 0x20, 0x30, 0x16, 0x7e, 0x06, 0x2f, 0x2e, 0xc7, 0x27, 0x4e, 0x81, 0xdf, 0x0e, 0x11, 0x18, 0xdd, 0x28, 0xa3, 0x72,
  0x54, 0xaa, 0x98, 0x4a, 0x08, 0x99, 0x7b, 0x14, 0x9e, 0xbc, 0x13, 0x4b, 0xae, 0x41, 0x2c, 0xda, 0x2b, 0x53, 0x39, 0xf6,
  0x09, 0x21, 0x0c, 0x02, 0xfe, 0x6e, 0x21, 0x9e, 0xb9, 0x1b, 0x27, 0xcf, 0x83, 0x1f, 0x82, 0x21, 0x6d, 0xb7, 0xab, 0x1c,
  0x95, 0x77, 0x6d, 0x40, 0x04, 0x1f, 0x70, 0xca, 0x49, 0x58, 0x7d, 0x1f, 0xae, 0xfa, 0x1c, 0x38, 0x7f, 0x40, 0xf4, 0xca,
  0x54, 0x8e, 0x26, 0x86, 0x90, 0x4a, 0xaf, 0xec, 0x1b, 0x5f, 0xc0, 0xca, 0xdb, 0x31, 0x67, 0x16, 0xfc, 0x0e, 0x58, 0xa3,
  0x59, 0xaa, 0xca, 0xb1, 0x67, 0xa1, 0x1b, 0x70, 0xd6, 0xa9, 0x78, 0xfe, 0x7e, 0x5c, 0xf2, 0x67, 0x08, 0x63, 0x90, 0x72,
  0xd2, 0x0a, 0x5d, 0x95, 0xa3, 0x85, 0x85, 0xae, 0x45, 0x08, 0xe8, 0xeb, 0xc6, 0xd2, 0xeb, 0xf1, 0xc0, 0x4d, 0xe8, 0xef,
  0x41, 0x18, 0x4d, 0x62, 0xaf, 0x4c, 0xe5, 0x68, 0x51, 0x08, 0xb1, 0x93, 0x21, 0xe4, 0xfc, 0x73, 0xf0, 0x93, 0x07, 0x71,
  0xde, 0x1f, 0x20, 0x0c, 0x27, 0xb0, 0x57, 0xa6, 0x72, 0xb4, 0x3a, 0x84, 0x1c, 0x7e, 0x28, 0x1e, 0xbd, 0x19, 0xb7, 0x2d,
  0x41, 0x47, 0x84, 0xb0, 0x53, 0x7b, 0x65, 0x2a, 0xc7, 0x1e, 0x21, 0x84, 0x05, 0xcc, 0x58, 0xf8, 0x27, 0x78, 0xfe, 0x7e,
  0xfc, 0xde, 0xa9, 0xf0, 0x3b, 0x20, 0xa2, 0x85, 0xae, 0xca, 0x51, 0xf9, 0x4c, 0x04, 0x63, 0xe0, 0x03, 0xe6, 0x1d, 0x8d,
  0x27, 0xef, 0xc4, 0x0d, 0x57, 0x23, 0x66, 0x70, 0x4e, 0x0b, 0x5d, 0x95, 0x63, 0x8f, 0x42, 0x97, 0x19, 0x10, 0x5c, 0x77,
  0x29, 0x7e, 0x7c, 0x2f, 0x3e, 0x32, 0x57, 0x7b, 0x65, 0x2a, 0xc7, 0x9e, 0x1f, 0x6e, 0xaa, 0x57, 0xf6, 0x5b, 0x27, 0x60,
  0xf5, 0x7d, 0xf8, 0xc2, 0xa5, 0xe0, 0x71, 0x70, 0x4e, 0x53, 0x10, 0x95, 0x63, 0xef, 0x10, 0x92, 0x49, 0xe1, 0x6b, 0x57,
  0xe1, 0xe9, 0x7b, 0xf1, 0xd1, 0xe3, 0x80, 0x12, 0x8c, 0xfa, 0xa1, 0x72, 0x4c, 0x55, 0x32, 0x93, 0xeb, 0xc7, 0x8e, 0x9a,
  0x85, 0xee, 0xce, 0x96, 0x3e, 0xf5, 0x47, 0xe5, 0x48, 0x14, 0x21, 0xc0, 0x10, 0xac, 0xc5, 0xad, 0x0f, 0x62, 0xee, 0xa7,
  0xf1, 0x5f, 0xcf, 0x01, 0x69, 0xb0, 0xde, 0x38, 0xe2, 0xd7, 0x88, 0xb9, 0xef, 0xe7, 0x0f, 0x57, 0x59, 0x82, 0x67, 0x2d,
  0x36, 0x0d, 0xe2, 0xb2, 0x2f, 0xe3, 0xf1, 0x27, 0x81, 0x2c, 0x4c, 0xeb, 0x16, 0xec, 0xaa, 0x1c, 0x09, 0x0a, 0x18, 0xd6,
  0xc2, 0x5a, 0xdc, 0xbf, 0x02, 0x57, 0x7f, 0x03, 0x03, 0x03, 0xb0, 0xdd, 0x60, 0x56, 0x33, 0x0e, 0x6e, 0x39, 0x2a, 0x6b,
  0x4e, 0x9d, 0xc5, 0x8e, 0x11, 0x5c, 0x73, 0x13, 0xee, 0xfa, 0x57, 0x20, 0x0d, 0xdb, 0x95, 0xb8, 0x65, 0xeb, 0x2a, 0x47,
  0xd3, 0x03, 0x06, 0xc3, 0x1a, 0x38, 0x8b, 0x95, 0xab, 0xb1, 0x68, 0x31, 0xde, 0x7e, 0x1b, 0xb6, 0x07, 0xc2, 0x8d, 0x36,
  0xa3, 0xb2, 0x6a, 0x7c, 0x9f, 0x6d, 0x6a, 0xd3, 0xfe, 0x70, 0xfa, 0x81, 0xbf, 0xc6, 0x0f, 0x55, 0x8e, 0xda, 0xf0, 0x01,
  0xce, 0xa2, 0x58, 0xc2, 0xb5, 0xdf, 0xc2, 0xb7, 0x97, 0x01, 0x06, 0xae, 0x17, 0xde, 0x37, 0xfa, 0xcf, 0x12, 0xc1, 0x45,
  0x93, 0x55, 0xd1, 0x9e, 0x58, 0x07, 0x00, 0x66, 0xff, 0xc7, 0x98, 0xcc, 0xd4, 0x42, 0x73, 0x4c, 0x37, 0xd0, 0xa8, 0x1c,
  0x75, 0xa1, 0xb2, 0x6b, 0xcc, 0x59, 0xac, 0x5e, 0x83, 0xbf, 0xfd, 0x0a, 0x5e, 0x5e, 0x03, 0xea, 0x06, 0x51, 0xa3, 0xcd,
  0x10, 0x81, 0x89, 0x30, 0xbe, 0x03, 0x0f, 0x5d, 0x39, 0x5d, 0x6b, 0x4d, 0x00, 0xc2, 0xd8, 0x56, 0xc4, 0xd9, 0x7d, 0xcb,
  0x23, 0x61, 0xb8, 0x14, 0x76, 0xac, 0xc3, 0xc3, 0x57, 0x4d, 0x57, 0x59, 0x0b, 0x40, 0x18, 0xd9, 0x8c, 0x28, 0xd3, 0xca,
  0xba, 0xca, 0xbd, 0x7f, 0x02, 0x46, 0x60, 0x7c, 0xed, 0x0e, 0xfc, 0xd3, 0x2d, 0x28, 0x95, 0xe0, 0xfa, 0xe0, 0x7d, 0x73,
  0x42, 0x33, 0x11, 0x7c, 0x11, 0x1b, 0x7e, 0x32, 0x7d, 0xf3, 0x44, 0x00, 0xeb, 0x60, 0xe3, 0x69, 0x26, 0x17, 0x32, 0x28,
  0x8e, 0x63, 0xdd, 0x4b, 0xd3, 0xb7, 0x5c, 0x04, 0x70, 0x11, 0x4c, 0xd4, 0xca, 0x1d, 0xd5, 0x07, 0xb8, 0x1c, 0x2c, 0x80,
  0xc0, 0x59, 0xbc, 0xfe, 0x36, 0x16, 0x2d, 0xc6, 0x33, 0xcf, 0x02, 0x5d, 0x30, 0xd9, 0x26, 0x4c, 0x25, 0xfb, 0xf8, 0x11,
  0x67, 0x7f, 0x45, 0x78, 0xd9, 0xaf, 0xa5, 0x64, 0x90, 0xca, 0xfe, 0xaa, 0xb0, 0xd4, 0xda, 0xbd, 0xf6, 0xee, 0x80, 0x0f,
  0x18, 0x20, 0xdc, 0xf2, 0x00, 0xae, 0xfd, 0x36, 0xc6, 0xc6, 0xe0, 0xfa, 0x10, 0x42, 0x4b, 0x02, 0x71, 0xd5, 0x9b, 0x5a,
  0x25, 0xc1, 0xdd, 0xb8, 0x03, 0x53, 0x0e, 0x11, 0xb0, 0xc0, 0x59, 0xac, 0x1f, 0xc0, 0xe7, 0x97, 0xe0, 0xb1, 0x95, 0x40,
  0x16, 0xb6, 0xa3, 0xc9, 0x01, 0x43, 0x4b, 0xd9, 0x04, 0x16, 0xab, 0x95, 0xee, 0x16, 0xe1, 0xc1, 0xff, 0xc0, 0xe7, 0x6f,
  0xc4, 0xe0, 0x54, 0x77, 0x4b, 0xdb, 0x18, 0x07, 0xb5, 0x1c, 0xbb, 0xba, 0x5b, 0x43, 0xa3, 0xb8, 0xfa, 0xeb, 0xb8, 0xfb,
  0x21, 0x20, 0x03, 0xdb, 0xad, 0x5a, 0x1c, 0xf4, 0x72, 0xec, 0xea, 0x6e, 0xfd, 0xe0, 0x59, 0x5c, 0x76, 0xc3, 0x64, 0x77,
  0x4b, 0x03, 0x86, 0xca, 0x31, 0x99, 0x7b, 0xe6, 0x0a, 0xf8, 0xfb, 0x9b, 0x71, 0xd3, 0x32, 0x80, 0x9a, 0xd3, 0xdd, 0x52,
  0x39, 0x12, 0x5e, 0xac, 0x4e, 0x75, 0xb7, 0x9e, 0x7f, 0x0d, 0x97, 0x7f, 0x05, 0xaf, 0xbc, 0xda, 0x9c, 0xee, 0x96, 0x82,
  0xa4, 0xaf, 0xe7, 0xf0, 0x01, 0xc6, 0x40, 0x04, 0xff, 0xbc, 0x14, 0x67, 0x7c, 0x16, 0xaf, 0xfc, 0x02, 0xae, 0x6f, 0xf2,
  0x42, 0xbc, 0x72, 0xf0, 0x46, 0x8e, 0x3d, 0xbb, 0x5b, 0x97, 0x5e, 0x8f, 0x55, 0xcf, 0x01, 0x9d, 0x30, 0xb1, 0x06, 0x8c,
  0x83, 0x3e, 0x72, 0x30, 0x4f, 0x2e, 0x13, 0xff, 0xee, 0xfd, 0x98, 0x7f, 0x21, 0x56, 0xbd, 0x04, 0xd7, 0x0b, 0x82, 0x06,
  0x0c, 0x8d, 0x1c, 0x82, 0x4c, 0x0a, 0x1b, 0x06, 0x70, 0xc5, 0x8d, 0x78, 0x64, 0x05, 0xd0, 0xa1, 0xdd, 0x2d, 0x95, 0x63,
  0xaa, 0x93, 0x81, 0x18, 0xeb, 0x06, 0xf1, 0xdb, 0x17, 0x61, 0xeb, 0x06, 0xd8, 0x5e, 0x2d, 0x56, 0x0f, 0x3c, 0x39, 0x1a,
  0x75, 0xab, 0x3b, 0x11, 0xc0, 0x61, 0xdb, 0x10, 0x00, 0xd8, 0x1e, 0xd5, 0xa2, 0x76, 0x2a, 0xb7, 0x26, 0x6c, 0x5e, 0xce,
  0x61, 0x80, 0x02, 0xc9, 0xbb, 0xc4, 0x00, 0x35, 0xe0, 0xaa, 0xa1, 0x80, 0x1c, 0xc8, 0xa9, 0x19, 0xb5, 0x1f, 0x49, 0x43,
  0xc8, 0xf9, 0xe1, 0xca, 0x73, 0xcb, 0xab, 0x78, 0x1e, 0x8f, 0xa9, 0x2e, 0x6c, 0x78, 0xc8, 0x76, 0xd3, 0xb0, 0x93, 0x97,
  0x90, 0x55, 0x72, 0xef, 0x8b, 0x72, 0x23, 0xef, 0x87, 0x4a, 0x9c, 0x23, 0xd8, 0x26, 0x45, 0x0e, 0x02, 0x01, 0xb2, 0x9e,
  0xca, 0x40, 0x95, 0xcf, 0x87, 0x52, 0x9a, 0x92, 0xbf, 0x89, 0x21, 0x8c, 0x16, 0x37, 0xf8, 0x66, 0x3e, 0x00, 0xb0, 0xc2,
  0x5b, 0xa6, 0xa4, 0x27, 0x20, 0xe1, 0x7a, 0x10, 0x61, 0x5b, 0xfe, 0x2d, 0x16, 0x6e, 0xde, 0xa3, 0x43, 0x19, 0x62, 0x61,
  0xfe, 0xc7, 0x16, 0x01, 0xb6, 0xba, 0xaf, 0x30, 0xc1, 0xc9, 0xa8, 0x08, 0xb6, 0x4c, 0xfc, 0x8c, 0x88, 0xd0, 0xc4, 0x07,
  0x00, 0x22, 0x2d, 0xf4, 0x0b, 0x2a, 0x6d, 0x36, 0x65, 0x6a, 0x48, 0x4e, 0xaa, 0xd4, 0x21, 0x6c, 0x58, 0xb2, 0x39, 0x5f,
  0xda, 0x3c, 0xfe, 0x8a, 0xa3, 0x94, 0x48, 0x13, 0xe5, 0x88, 0x80, 0x1d, 0x26, 0xac, 0xb6, 0x05, 0x01, 0xe9, 0xa3, 0x65,
  0x93, 0x99, 0x70, 0x44, 0x06, 0x03, 0x13, 0x3f, 0xdd, 0x5e, 0x78, 0x2b, 0x32, 0x69, 0x69, 0xda, 0x03, 0x00, 0x31, 0xf5,
  0x2c, 0xfb, 0xef, 0xbb, 0x71, 0x9d, 0x54, 0x12, 0x2a, 0x07, 0xd8, 0x19, 0xbc, 0x3e, 0xf4, 0x44, 0x31, 0xe4, 0x89, 0xaa,
  0xbc, 0x6b, 0x5e, 0x95, 0x72, 0x04, 0x48, 0x46, 0xcc, 0x0f, 0x6d, 0x6e, 0x1b, 0x95, 0x2d, 0x8c, 0x5e, 0xf3, 0x48, 0x5e,
  0x87, 0xc3, 0xe6, 0x7c, 0xf9, 0xe7, 0x43, 0x8f, 0x47, 0x26, 0x25, 0xd5, 0x2e, 0x62, 0x36, 0xd5, 0x8a, 0x89, 0x34, 0x68,
  0x8b, 0xf1, 0x0f, 0x45, 0x63, 0xd0, 0x99, 0x25, 0x61, 0xb0, 0x70, 0xda, 0xd2, 0x9b, 0xc3, 0x4f, 0x6d, 0x1a, 0x7f, 0x35,
  0x65, 0xb3, 0xd5, 0xcd, 0x29, 0x35, 0x95, 0xb2, 0x0c, 0x89, 0x40, 0x77, 0x46, 0x3b, 0x4b, 0x60, 0xa3, 0x35, 0x4b, 0xa2,
  0xaa, 0x14, 0x90, 0x00, 0xcf, 0x0d, 0x2e, 0xad, 0xb1, 0x0b, 0x55, 0x8b, 0x1c, 0xc8, 0x8a, 0x79, 0xc5, 0xe6, 0x1f, 0x8e,
  0xc6, 0x0c, 0x6c, 0xd0, 0xe0, 0x91, 0x94, 0x54, 0x34, 0xa4, 0x9d, 0xf9, 0xdf, 0x91, 0xe7, 0x5e, 0x1f, 0x5a, 0x91, 0xb1,
  0x9d, 0x5c, 0xc3, 0xc3, 0xc0, 0x6a, 0x5a, 0xcf, 0x21, 0x90, 0x94, 0x98, 0xc5, 0xa9, 0x1d, 0x13, 0xe4, 0xb5, 0xa6, 0x4d,
  0x4e, 0xdc, 0x60, 0xc1, 0x0f, 0xd6, 0xff, 0x03, 0x4b, 0xa8, 0xae, 0x31, 0x5a, 0x1f, 0x39, 0x18, 0x68, 0x83, 0x79, 0xc3,
  0x14, 0xbf, 0x1a, 0xef, 0xd0, 0xe0, 0x91, 0x04, 0x82, 0x84, 0xac, 0xb3, 0xcf, 0x0d, 0xdc, 0xf3, 0xc6, 0xf0, 0x53, 0x69,
  0x57, 0x53, 0xd8, 0x40, 0xed, 0x2b, 0xc1, 0x3c, 0xa4, 0x53, 0xec, 0xd7, 0x53, 0xc3, 0xab, 0xed, 0x84, 0x53, 0x3f, 0x5a,
  0x3c, 0xa1, 0x70, 0xca, 0xda, 0x81, 0xdc, 0xba, 0xef, 0xaf, 0xbb, 0x36, 0xb6, 0x59, 0xa9, 0x79, 0xa7, 0x65, 0x1d, 0x96,
  0x09, 0x56, 0x72, 0xd1, 0xbf, 0xce, 0x0c, 0x0e, 0x53, 0x30, 0x5a, 0xd6, 0xb6, 0xac, 0xb1, 0x21, 0x44, 0x22, 0xe2, 0x97,
  0xbf, 0x71, 0xc9, 0x44, 0x79, 0xbb, 0x33, 0x71, 0xed, 0xd7, 0x44, 0xeb, 0x20, 0x07, 0x03, 0x6d, 0x62, 0xde, 0x30, 0xa5,
  0x8b, 0xd3, 0x03, 0x95, 0xcc, 0x43, 0xa3, 0x47, 0x4b, 0xf2, 0xd0, 0xb6, 0xc8, 0x3e, 0xf6, 0xf6, 0xb5, 0x6f, 0x8c, 0x3c,
  0x95, 0x71, 0xdd, 0x5c, 0x8f, 0x87, 0xd2, 0xd6, 0x67, 0x81, 0xb1, 0x87, 0xf4, 0x88, 0x7d, 0x34, 0x1a, 0x5b, 0x94, 0x1e,
  0xb4, 0x30, 0xea, 0x47, 0x93, 0xc5, 0x60, 0xf1, 0x1d, 0xb1, 0x7b, 0xe2, 0x9d, 0x1b, 0x9f, 0xd9, 0xf4, 0xcd, 0xf6, 0xa8,
  0x97, 0xa5, 0x3e, 0x4b, 0x6e, 0xeb, 0xb6, 0xfa, 0xdc, 0x43, 0x7a, 0xc4, 0xdd, 0x1a, 0x0f, 0xff, 0x63, 0x6a, 0x9b, 0x81,
  0x11, 0x40, 0xe7, 0x97, 0xe6, 0xcc, 0x26, 0x22, 0x21, 0x1b, 0xb9, 0xff, 0xde, 0x74, 0xc7, 0x8a, 0x75, 0x5f, 0xca, 0xb8,
  0x6e, 0xae, 0xdf, 0x4d, 0x1d, 0xea, 0xb9, 0x35, 0xa1, 0xe2, 0xc7, 0xe2, 0xd4, 0xf6, 0xbf, 0x49, 0x0f, 0x1a, 0x18, 0x03,
  0xd2, 0xfc, 0xb4, 0xa1, 0xb0, 0x04, 0x03, 0xca, 0x46, 0x6e, 0xc5, 0x3b, 0x4b, 0x96, 0xbf, 0xf9, 0xb9, 0xb4, 0xeb, 0x9c,
  0x14, 0x26, 0x81, 0x72, 0xec, 0xf2, 0xe3, 0xb6, 0x78, 0xe4, 0xdc, 0xb6, 0x4d, 0x5b, 0x29, 0x58, 0x58, 0xaf, 0x2b, 0xfe,
  0x1a, 0x13, 0x32, 0x82, 0xf8, 0xb4, 0xb3, 0x82, 0xfc, 0xb2, 0x5f, 0x5e, 0xba, 0x62, 0xdd, 0x97, 0xb2, 0xae, 0x07, 0x22,
  0xf5, 0x5d, 0x98, 0x57, 0xff, 0x4d, 0x4d, 0x1e, 0xd2, 0x23, 0xe6, 0x71, 0x37, 0x7e, 0x7a, 0x76, 0xfd, 0x93, 0x6e, 0xc2,
  0xc1, 0x11, 0x48, 0x15, 0xa9, 0x6f, 0xc0, 0x20, 0x50, 0x47, 0xe4, 0x36, 0x8e, 0xbd, 0xfa, 0x9d, 0xd7, 0x7e, 0xf7, 0xd9,
  0x81, 0xdb, 0xb3, 0x51, 0xaf, 0x80, 0xeb, 0xbe, 0x64, 0xb3, 0x21, 0x3b, 0xde, 0x3c, 0xd0, 0x23, 0x76, 0x23, 0xf9, 0x05,
  0x6d, 0x9b, 0xae, 0x4c, 0x0f, 0x6e, 0xa7, 0xe0, 0xe0, 0x2a, 0xcb, 0x92, 0x35, 0x11, 0xa9, 0x25, 0xbd, 0xa8, 0xd4, 0x20,
  0xd9, 0xc8, 0x32, 0x72, 0x4f, 0xac, 0xff, 0xea, 0xcd, 0xaf, 0x7d, 0x7c, 0xe3, 0xf8, 0x2b, 0xed, 0x51, 0x5f, 0x5d, 0x6a,
  0x93, 0xff, 0x4f, 0xa3, 0x36, 0x35, 0x79, 0x48, 0x1a, 0x24, 0x82, 0x6f, 0xc5, 0xc3, 0x8f, 0xb9, 0xf1, 0xab, 0x4b, 0xbd,
  0x17, 0x95, 0x3b, 0x3b, 0xc4, 0x01, 0xcc, 0x10, 0x86, 0x54, 0x1e, 0xff, 0xaa, 0xd7, 0xeb, 0xde, 0xbb, 0x44, 0x85, 0x88,
  0x08, 0x40, 0x91, 0x31, 0xb1, 0xb3, 0x05, 0xef, 0x5f, 0xdc, 0xfa, 0xf0, 0x0f, 0x37, 0xdc, 0xb8, 0x71, 0x7c, 0x4d, 0x9b,
  0xeb, 0x4c, 0xdb, 0x8e, 0x7a, 0xd5, 0x26, 0xcd, 0x93, 0x03, 0x53, 0xd5, 0x4a, 0x8f, 0xd8, 0x01, 0xf2, 0x97, 0xa7, 0xb7,
  0x7e, 0x27, 0x1e, 0xbe, 0xb0, 0xdc, 0x79, 0x7e, 0xb9, 0x73, 0x0e, 0xc7, 0x66, 0x32, 0x6f, 0x12, 0x06, 0x2a, 0x97, 0xfb,
  0x09, 0x68, 0xb9, 0x2a, 0x01, 0xc2, 0x12, 0x58, 0x02, 0xa4, 0xe5, 0xcf, 0xdc, 0xa8, 0x4c, 0x10, 0x64, 0xc8, 0x46, 0x86,
  0x22, 0x03, 0x16, 0x0c, 0x15, 0xb6, 0xfc, 0x74, 0xfb, 0xc3, 0x2f, 0x6d, 0x5d, 0xb6, 0x61, 0xfc, 0x65, 0x47, 0x71, 0x25,
  0x60, 0x34, 0x28, 0x66, 0x54, 0xa0, 0x8e, 0x8e, 0xae, 0x46, 0x7f, 0x50, 0x02, 0x2c, 0x28, 0x07, 0x2e, 0x12, 0xf7, 0x8a,
  0x9b, 0x1f, 0xd2, 0x0b, 0xca, 0xed, 0xa7, 0x87, 0xcc, 0xb1, 0x1c, 0xc7, 0x30, 0x53, 0xa7, 0xa1, 0xb5, 0x39, 0x09, 0x01,
  0x1e, 0x51, 0xcf, 0x3d, 0x67, 0xae, 0xcc, 0xc7, 0xc6, 0x48, 0x8b, 0xdf, 0x4a, 0xe5, 0x7e, 0xb7, 0x2c, 0x28, 0x06, 0x1e,
  0x2a, 0xac, 0xdd, 0x30, 0xf6, 0xfc, 0xcf, 0x87, 0x56, 0xac, 0x1d, 0x5d, 0x35, 0x5a, 0xdc, 0xe2, 0x4c, 0x9c, 0xb2, 0xed,
  0x22, 0x22, 0x8d, 0xef, 0x15, 0x34, 0x43, 0x8e, 0x5d, 0xd9, 0x8d, 0x01, 0x95, 0x20, 0x39, 0x62, 0x01, 0xb2, 0x62, 0x3e,
  0xc4, 0xd1, 0xb1, 0x1c, 0x1f, 0xcd, 0xd1, 0x6c, 0x89, 0x0e, 0x63, 0x3b, 0x43, 0x6c, 0x4b, 0xbf, 0xb0, 0xa1, 0xec, 0x3a,
  0xfe, 0xf3, 0xc4, 0x1b, 0x4a, 0xae, 0x8d, 0x5a, 0x17, 0x39, 0x88, 0xa8, 0xe0, 0x47, 0xc6, 0x4a, 0x03, 0x23, 0xa5, 0x4d,
  0xdb, 0xf3, 0x6b, 0xb7, 0xe5, 0xdf, 0xdc, 0x9e, 0x5f, 0x9b, 0xf3, 0xc3, 0x44, 0x14, 0x9b, 0xac, 0x33, 0x29, 0x11, 0x96,
  0x66, 0xb5, 0x90, 0x9a, 0x27, 0xc7, 0xae, 0xaf, 0x85, 0x01, 0x2a, 0xf5, 0x4b, 0x11, 0x52, 0xda, 0xdd, 0x6d, 0x27, 0xdb,
  0xd2, 0xe8, 0x41, 0x00, 0x04, 0x8e, 0x0b, 0x2d, 0x0d, 0x61, 0x42, 0x44, 0x2c, 0x81, 0x85, 0x05, 0x30, 0x20, 0x6b, 0x62,
  0x67, 0x52, 0x96, 0x1c, 0x00, 0x16, 0x6e, 0xf2, 0x11, 0x6a, 0xb6, 0x1c, 0xfb, 0xc4, 0x12, 0x9a, 0xdc, 0x3f, 0x87, 0x84,
  0x6c, 0x80, 0x14, 0x98, 0x96, 0x27, 0x1b, 0xb4, 0x3b, 0x53, 0x17, 0x99, 0x6c, 0x5d, 0xb4, 0xe6, 0xd8, 0xb4, 0xf2, 0x16,
  0x0c, 0xbc, 0x57, 0xf2, 0x95, 0x94, 0x26, 0x42, 0xcb, 0xdf, 0x4e, 0x72, 0x3a, 0x42, 0xfa, 0x90, 0x55, 0x45, 0xe5, 0x50,
  0x54, 0x0e, 0x45, 0xe5, 0x50, 0x54, 0x0e, 0x45, 0xe5, 0x50, 0x54, 0x0e, 0x45, 0xe5, 0x50, 0x54, 0x0e, 0x45, 0xe5, 0x50,
  0x54, 0x0e, 0x45, 0x51, 0x39, 0x14, 0x95, 0x43, 0x51, 0x39, 0x14, 0x95, 0x43, 0x51, 0x39, 0x14, 0x95, 0x43, 0x51, 0x39,
  0x14, 0x95, 0x43, 0x51, 0x39, 0x14, 0x95, 0x43, 0x51, 0x54, 0x0e, 0x45, 0xe5, 0x50, 0x54, 0x0e, 0x45, 0xe5, 0x50, 0x54,
  0x0e, 0x45, 0xe5, 0x50, 0x54, 0x0e, 0x45, 0xe5, 0x50, 0x54, 0x0e, 0x45, 0xe5, 0x50, 0x14, 0x95, 0x43, 0x51, 0x39, 0x14,
  0x95, 0x43, 0x51, 0x39, 0x14, 0x95, 0x43, 0x51, 0x39, 0x94, 0x84, 0xf2, 0x7f, 0xa0, 0x1b, 0x7e, 0x84, 0x42, 0x4f, 0x8a,
  0xa0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

void sendIcon(const char *type, const uint8_t *data, size_t len) {
  server.sendHeader("Cache-Control", "public, max-age=604800");
  server.send_P(200, type, (const char *)data, len);
}
void handleIconSvg() { sendIcon("image/svg+xml", (const uint8_t *)ICON_SVG, strlen_P(ICON_SVG)); }
void handleIconPng() { sendIcon("image/png", ICON_PNG32, sizeof(ICON_PNG32)); }
void handleTouchIcon() { sendIcon("image/png", ICON_PNG180, sizeof(ICON_PNG180)); }
#define ICON_LINKS "<link rel='icon' href='/favicon.svg' type='image/svg+xml'><link rel='icon' href='/favicon.png' sizes='32x32' type='image/png'><link rel='apple-touch-icon' href='/apple-touch-icon.png'>"

const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter setup</title>)HTML" ICON_LINKS R"HTML(<style>
body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:20px}
.card{max-width:440px;margin:0 auto;background:#1c1c1e;border-radius:14px;padding:22px}
h1{font-size:22px;margin:0 0 4px}p{color:#999;font-size:14px;margin:0 0 18px}
label{display:block;font-size:13px;color:#aaa;margin:14px 0 6px}
input,select,textarea{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #333;background:#000;color:#fff;font-size:16px;font-family:inherit}
textarea{min-height:150px}
button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:10px;background:#e62117;color:#fff;font-size:17px;font-weight:600}
small{display:block;color:#777;font-size:12px;margin-top:6px}a{color:#4ea1ff}
.st{font-size:13px;color:#aaa;margin-top:6px}.st b{color:#fff}
h2{font-size:17px;margin:30px 0 4px;padding-top:16px;border-top:1px solid #2a2a2e;scroll-margin-top:12px}
.jump{display:flex;flex-wrap:wrap;gap:6px;margin:0 0 14px}.jump a{font-size:13px;background:#2a2a2e;color:#ddd;border-radius:999px;padding:5px 10px;text-decoration:none}
summary{margin-top:14px;cursor:pointer;color:#aaa;font-size:14px}
.row{display:flex;gap:8px;align-items:center}.row span{color:#aaa}
.b2{background:#2a2a2e;font-size:15px;padding:12px;margin-top:10px}
.mini{width:auto;margin:0;padding:5px 10px;font-size:13px;background:#2a2a2e}
.btnlink{display:block;text-align:center;margin-top:10px;padding:12px;border-radius:10px;background:#1db954;color:#fff;font-weight:600;text-decoration:none}
.saved{display:flex;flex-wrap:wrap;gap:6px 14px;align-items:center;padding:8px 0;border-top:1px solid #333}.saved b{flex:1 1 100%;word-break:break-word;color:#fff}
.ok{color:#30d158;margin:6px 0}
.rpv{display:flex;flex-wrap:wrap;gap:8px 12px;align-items:center;margin-top:10px;font-size:15px}.rpv span{display:inline-flex;align-items:center;gap:6px;background:#111;border:1px solid #333;border-radius:999px;padding:5px 12px}.rpv em{color:#777;font-style:normal}
.secret{-webkit-text-security:disc;text-security:disc}
.savebar{position:sticky;bottom:0;background:#1c1c1e;padding:6px 0 10px;margin-top:22px;box-shadow:0 -10px 14px #1c1c1e}.savebar button{margin-top:6px}
</style></head><body><div class="card">)HTML";


const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter</title>
)HTML" ICON_LINKS R"HTML(
<style>
:root{--bg:#0d0d0f;--card:#18181b;--line:#2a2a2e;--text:#f2f2f3;--muted:#8d8d95;--red:#ff3b30;--gold:#ffc53d;--green:#30d158;--blue:#4da3ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,system-ui,Segoe UI,Roboto,sans-serif}
a{color:inherit;text-decoration:none}
header{display:flex;align-items:center;gap:12px;padding:16px 20px;border-bottom:1px solid var(--line);position:sticky;top:0;background:rgba(13,13,15,.92);backdrop-filter:blur(8px);z-index:2}
.logo{width:34px;height:24px;flex:none;display:block}
header h1{font-size:18px;margin:0;flex:1}header .meta{color:var(--muted);font-size:13px}
.btn{border:1px solid var(--line);border-radius:9px;padding:7px 12px;font-size:13px;color:var(--text);background:var(--card);cursor:pointer}
main{max-width:1500px;margin:0 auto;padding:20px}
.layout{display:grid;grid-template-columns:340px minmax(0,1fr);gap:16px;align-items:start}
.side{display:grid;gap:16px;align-content:start}
.grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));align-items:start}
@media(max-width:1000px){.layout{grid-template-columns:1fr}.side{grid-template-columns:repeat(auto-fit,minmax(280px,1fr));align-items:start}}
@media(max-width:600px){main{padding:12px}.side,.grid{grid-template-columns:1fr}.big{font-size:38px}.vid img{width:104px}header{padding:12px}header .meta{white-space:nowrap;font-size:12px}}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:18px}
.card h2{font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--gold);margin:0 0 12px}
.err{background:#3a1210;border-color:#e62117}
.av{width:44px;height:44px;border-radius:50%;object-fit:cover;background:#333;flex:none}
.av.lg{width:64px;height:64px}
.avw{position:relative;display:inline-flex;flex:none;border-radius:50%}
.avw.on{box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--red)}.avw.on.lg{box-shadow:0 0 0 3px var(--card),0 0 0 6px var(--red)}
.avw .lv{position:absolute;left:50%;bottom:-7px;transform:translateX(-50%);background:var(--red);color:#fff;border:2px solid var(--card);border-radius:5px;padding:0 4px;font-size:9px;line-height:13px;font-weight:800;letter-spacing:.03em}
.avw.lg .lv{font-size:11px;line-height:15px;padding:0 6px;bottom:-9px}
td .avw .lv{font-size:7px;line-height:10px;padding:0 3px;bottom:-5px;border-width:1px}td .avw.on{box-shadow:0 0 0 1px var(--card),0 0 0 3px var(--red)}
.top{display:flex;gap:12px;align-items:center}.top .nm{font-weight:700;font-size:17px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sub{color:var(--muted);font-size:13px}
.big{font-size:44px;font-weight:800;letter-spacing:-.02em;margin:10px 0 2px;font-variant-numeric:tabular-nums}
.est{font-size:12px;color:var(--muted);font-weight:500;margin-left:6px;letter-spacing:0}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}
.chip{background:#222226;border-radius:9px;padding:6px 10px;font-size:13px}.chip b{font-size:15px}
.pos{color:var(--green)}.neg{color:var(--red)}
.bar{height:10px;border-radius:6px;background:#2a2a2e;overflow:hidden;margin:8px 0 4px}.bar i{display:block;height:100%;background:var(--gold);border-radius:6px}
.vid{display:flex;gap:12px;margin-top:14px;padding-top:14px;border-top:1px solid var(--line)}
.vid img{width:128px;aspect-ratio:16/9;object-fit:cover;border-radius:8px;flex:none;background:#333}
.vid .t{font-weight:600;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.live{background:var(--red);color:#fff;border-radius:5px;padding:1px 6px;font-size:11px;font-weight:800;margin-right:6px}
svg.g{width:100%;height:120px;display:block;margin-top:12px}
.tabs{display:flex;gap:6px;margin-top:12px}.tabs button{border:0;border-radius:8px;padding:5px 12px;font-size:13px;background:#222226;color:var(--muted);cursor:pointer}.tabs button.on{background:#3a3a40;color:var(--text)}
table{width:100%;border-collapse:collapse}td{padding:8px 4px;border-top:1px solid var(--line)}td.n{text-align:right;font-variant-numeric:tabular-nums}
tr.me td{color:var(--gold)}
th{font-size:11px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;color:var(--muted);padding:0 4px 6px}th.n{text-align:right}tr.hd+tr td{border-top:0}
.race .rs{display:flex;justify-content:space-between;align-items:center;margin:8px 0}.race .rs span{display:flex;align-items:center;gap:10px;min-width:0}
td .av{width:28px;height:28px;display:block}td{padding:6px 4px}td.nm2{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:150px}
.tug{display:flex;height:14px;border-radius:7px;overflow:hidden;margin:10px 0}.tug .a{background:var(--red)}.tug .b{background:var(--blue)}
.list div{display:flex;justify-content:space-between;padding:5px 0}
.wide{grid-column:1/-1}
.pi{display:inline-flex;vertical-align:-2px;margin-right:6px}.pi svg{display:block}
.pf{max-width:1500px;margin:0 auto;padding:14px 20px 0;display:flex;gap:8px}.pf button{border:1px solid var(--line);background:var(--card);color:var(--muted);border-radius:999px;padding:7px 14px;font-size:14px;cursor:pointer}
.pf button.on{color:var(--text);border-color:var(--gold);background:#222226}.pf button span{opacity:.6;margin-left:2px;font-size:12px}
@media(max-width:600px){.pf{padding:12px 12px 0}}
.twb{background:#9146ff;color:#fff;border-radius:5px;font-size:10px;font-weight:800;padding:1px 5px;margin-left:8px;vertical-align:3px;letter-spacing:.03em}
.ctl{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:12px}
.tg{display:inline-flex;align-items:center;gap:6px;border:1px solid var(--line);border-radius:999px;padding:4px 10px;font-size:13px;background:none;color:var(--muted);cursor:pointer}
.tg.on{color:var(--text);border-color:#4a4a52;background:#222226}.tg i{width:10px;height:10px;border-radius:50%;display:inline-block}
.seg{display:inline-flex;background:#222226;border-radius:9px;padding:2px}.seg button{border:0;background:none;color:var(--muted);padding:4px 10px;border-radius:7px;font-size:13px;cursor:pointer}.seg button.on{background:#3a3a40;color:var(--text)}
.cmp{position:relative}.cmp svg{width:100%;height:260px;display:block}
.tip{position:absolute;pointer-events:none;background:#0d0d0f;border:1px solid var(--line);border-radius:10px;padding:8px 10px;font-size:12px;white-space:nowrap;display:none;z-index:1}
.tip div{display:flex;gap:8px;align-items:center}.tip i{width:8px;height:8px;border-radius:50%;display:inline-block}
.small{font-size:12px;color:var(--muted);text-decoration:underline;margin-left:auto}
.cm{margin-top:14px;padding-top:12px;border-top:1px solid var(--line)}.cm h3{font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);margin:0 0 6px}
.cm .c{padding:6px 0}.cm .c b{font-size:13px}.cm .c p{margin:2px 0 0;font-size:14px;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.rvt td.nm2{max-width:200px}.rvt th:first-child{text-align:left}.rvt th a{color:inherit;text-decoration:none}details.cm summary{cursor:pointer;list-style:revert}
.pb{display:flex;flex-wrap:wrap;gap:6px 14px;font-size:13px;color:var(--muted);margin-top:10px}.pb b{color:var(--gold)}
</style></head><body>
<header><svg class="logo" viewBox="2 11 60 42" aria-hidden="true"><defs><clipPath id="lc"><rect x="2" y="11" width="60" height="42" rx="12"/></clipPath></defs><g clip-path="url(#lc)"><rect x="2" y="11" width="30" height="42" fill="#ff0033"/><rect x="32" y="11" width="30" height="42" fill="#9146ff"/></g><path d="M15 23v18l13-9z" fill="#fff"/><path d="M40 24h4v11h-4zM48 24h4v11h-4z" fill="#fff"/></svg><h1>SubCounter</h1><span class="meta" id="meta"></span><a class="btn" id="upd" href="/update" style="display:none;border-color:#30d158;color:#30d158"></a><a class="btn" href="/settings">Settings</a></header>
<nav id="pf" class="pf" style="display:none"></nav><main id="app"><div class="card">Loading…</div></main>
<script>
const $=s=>document.querySelector(s);
function h(t,a,...k){const e=document.createElement(t);for(const x in a||{}){if(x=='class')e.className=a[x];else if(x=='text')e.textContent=a[x];else if(x.startsWith('on'))e[x]=a[x];else e.setAttribute(x,a[x])}for(const c of k.flat())if(c!=null)e.append(c.nodeType?c:document.createTextNode(c));return e}
const fmt=n=>n==null||n<0?'-':Number(n).toLocaleString('en-GB');
const cmp=n=>{if(n==null||n<0)return'-';if(n<10000)return fmt(n);const u=['K','M','B'];let i=-1;while(n>=1000&&i<2){n/=1000;i++}return(n>=100?n.toFixed(0):n>=10?n.toFixed(1):n.toFixed(2))+u[i]};
const sg=n=>(n>=0?'+':'')+fmt(n);
const cls=n=>n>0?'pos':n<0?'neg':'';
function ago(t){if(!t)return'';let d=Date.now()/1000-t;const p=(n,w)=>n+' '+w+(n==1?'':'s')+' ago';if(d<3600)return Math.max(1,d/60|0)+' min ago';if(d<86400)return(d/3600|0)+' h ago';if(d<2592000)return p(d/86400|0,'day');return p(d/2592000|0,'month')}
function dur(s){if(!s)return'';const p=x=>String(x).padStart(2,'0');return s>=3600?`${s/3600|0}:${p((s/60|0)%60)}:${p(s%60)}`:`${s/60|0}:${p(s%60)}`}
let D=null,graphs={};
function est(c){if(!D.est||!c.rate||c.rate<=0||c.step<=1||!c.stepAt)return c.subs;return c.subs+Math.min(c.step-1,Math.max(0,Math.floor(c.rate*(Date.now()/1000-c.stepAt)/86400)))}
function eta(c){const e=est(c);if(!(c.rate>0.01))return c.rate<0?'Losing subscribers':'Need more data for a date';const d=(c.next-e)/c.rate;if(d<1)return'Expected today';if(d>3650)return'10+ years away';return'Expected around '+new Date(Date.now()+d*864e5).toLocaleDateString('en-GB',{day:'numeric',month:'short',year:d>300?'numeric':undefined})}
function av(c,lg){const i=c.avatar?h('img',{class:'av'+(lg?' lg':''),src:c.avatar,alt:''}):h('div',{class:'av'+(lg?' lg':'')});const L=isLive(c);return h('span',{class:'avw'+(L?' on':'')+(lg?' lg':''),title:L?name(c)+' is live now':''},i,L?h('span',{class:'lv',text:'LIVE'}):null)}
function isLive(c){return !!(c.vid&&c.vid.live)}
function name(c){return c.title||c.handle}
async function graph(c,days,box){box.textContent='';const r=await fetch('/api/history?i='+c.i+'&days='+days);const pts=await r.json();if(pts.length<2){box.append(h('div',{class:'sub',text:'Collecting data – recorded every hour'}));return}
pts.push([Date.now()/1000|0,c.subs]);const W=600,H=120,t0=pts[0][0],t1=pts[pts.length-1][0];let mn=Math.min(...pts.map(p=>p[1])),mx=Math.max(...pts.map(p=>p[1]));if(mx==mn){mx++;mn--}
const X=t=>(t-t0)/(t1-t0||1)*(W-4)+2,Y=v=>H-6-(v-mn)/(mx-mn)*(H-24);const d=pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg');svg.setAttribute('viewBox',`0 0 ${W} ${H}`);svg.setAttribute('class','g');svg.setAttribute('preserveAspectRatio','none');
const area=document.createElementNS(ns,'path');area.setAttribute('d',d+` L ${X(t1)} ${H} L ${X(t0)} ${H} Z`);area.setAttribute('fill','rgba(48,209,88,.12)');svg.append(area);
const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#30d158');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');svg.append(ln);
box.append(svg,h('div',{class:'sub',text:`${cmp(mn)} – ${cmp(mx)}  ·  ${new Date(t0*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'})} to now`}))}
function channelCard(c){const card=h('div',{class:'card'+(c.err&&c.subs<0?' err':'')});
const link=c.tw?'https://www.twitch.tv/'+c.tw:isLive(c)?'https://youtu.be/'+c.vid.id:c.id?'https://www.youtube.com/channel/'+c.id:'#';
card.append(h('a',{class:'top',href:link,target:'_blank'},av(c,true),h('div',{style:'min-width:0'},h('div',{class:'nm'},name(c),c.tw?h('span',{class:'twb',text:'Twitch'}):null),h('div',{class:'sub',text:[c.tw?'twitch.tv/'+c.tw:c.handle,c.country,c.joined?'since '+new Date(c.joined*1000).getFullYear():''].filter(Boolean).join(' · ')}))));
if(c.err&&c.subs<0){card.append(h('p',{text:c.err}));return card}
const e=est(c);card.append(h('div',{class:'big'},h('span',{'data-est':c.i,text:fmt(e)}),(D.est&&c.step>1&&c.rate>0)?h('span',{class:'est',text:'est.'}):null));
card.append(h('div',{class:'sub',text:c.tw?(isLive(c)?'followers · live now':'followers · offline'):`${cmp(c.views)} views · ${fmt(c.videos)} videos · ${c.videos>0?cmp(Math.round(c.views/c.videos)):'-'} avg`}));
if(c.statsOk)card.append(h('div',{class:'chips'},[['Today',c.today],['Last 24 h',c.d1],['7 days',c.d7],['30 days',c.d30]].map(([k,v])=>h('div',{class:'chip'},k+' ',h('b',{class:cls(v),text:sg(v)}))),c.rate?h('div',{class:'chip'},'≈ ',h('b',{text:sg(Math.round(c.rate))}),'/day'):null));
else card.append(h('div',{class:'chips'},h('div',{class:'chip',text:'Growth: collecting data'})));
const f=Math.max(0,Math.min(1,(e-c.prev)/(c.next-c.prev||1)));
card.append(h('div',{style:'margin-top:6px;display:flex;justify-content:space-between'},h('span',{class:'sub',text:'Next milestone '}),h('b',{text:fmt(c.next)})),h('div',{class:'bar'},h('i',{style:`width:${(f*100).toFixed(1)}%`})),(c.next-e>0&&c.next-e<=Math.max(10,(c.next-c.prev)/10))?h('div',{style:'color:var(--gold);font-weight:700',text:`Almost there: ${fmt(c.next-e)} to go! · ${eta(c)}`}):h('div',{class:'sub',text:`${fmt(c.next-e)} to go · ${eta(c)}`}));
const tabs=h('div',{class:'tabs'}),gbox=h('div');let cur=graphs[c.i]||7;
for(const dd of [7,30]){const b=h('button',{text:dd+' days',class:dd==cur?'on':'',onclick:()=>{graphs[c.i]=dd;[...tabs.children].forEach(x=>x.className='');b.className='on';graph(c,dd,gbox)}});tabs.append(b)}
tabs.append(h('a',{class:'small',href:'/api/csv?i='+c.i,text:'Download CSV'}));card.append(tabs,gbox);graph(c,cur,gbox);
if(c.vt){const t=c.vt,ag=t.ageMin<60?t.ageMin+' min':(t.ageMin/60|0)+'h '+(t.ageMin%60)+'m';const box=h('div',{style:'margin-top:14px;padding:12px;border-radius:12px;background:#1f2a1f'},h('div',{class:'sub',style:'color:var(--gold)',text:'NEW VIDEO TRACKER'}),h('div',{style:'font-size:26px;font-weight:800',text:fmt(t.views)+' views'}),h('div',{class:'sub',text:`in ${ag}`+(t.rate>=0?` · ${cmp(t.rate)}/hour now`:'')}),t.cmp?h('div',{style:'margin-top:4px',class:t.cmp.includes('+')?'pos':t.cmp.includes('-')?'neg':'',text:t.cmp}):null);
if(t.pts&&t.pts.length>1){const W=600,H=70,p=t.pts,t0=p[0][0],t1=p[p.length-1][0],mn=p[0][1],mx=Math.max(p[p.length-1][1],mn+1);const d=p.map((q,k)=>(k?'L':'M')+((q[0]-t0)/(t1-t0||1)*(W-4)+2).toFixed(1)+' '+(H-4-(q[1]-mn)/(mx-mn)*(H-10)).toFixed(1)).join(' ');const ns='http://www.w3.org/2000/svg',sv=document.createElementNS(ns,'svg');sv.setAttribute('viewBox',`0 0 ${W} ${H}`);sv.setAttribute('preserveAspectRatio','none');sv.setAttribute('class','g');sv.style.height='70px';const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#ffc53d');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');sv.append(ln);box.append(sv)}
card.append(box)}
if(c.vid){const v=c.vid;card.append(h('a',{class:'vid',href:c.tw?'https://www.twitch.tv/'+c.tw:'https://youtu.be/'+v.id,target:'_blank'},h('img',{src:v.thumb||`https://i.ytimg.com/vi/${v.id}/mqdefault.jpg`,alt:''}),h('div',{style:'min-width:0'},h('div',{class:'t'},v.live?h('span',{class:'live',text:'LIVE'}):null,v.title),h('div',{class:'sub',text:v.live?`${fmt(v.viewers)} watching now`:`${ago(v.pub)} · ${dur(v.dur)}`}),c.tw?h('div',{class:'sub',text:v.game||''}):h('div',{class:'sub',text:`${cmp(v.views)} views · ${v.likes>=0?cmp(v.likes)+' likes':'likes hidden'} · ${v.comments>=0?cmp(v.comments)+' comments':'comments off'}`}))))}
if(c.pb){const b=c.pb,dd=t=>t?new Date(t*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'}):'';const it=[];
if(b.day)it.push(h('span',{},'Best day ',h('b',{text:sg(b.day)}),' '+dd(b.dayT)));if(b.week)it.push(h('span',{},'Best week ',h('b',{text:sg(b.week)}),' '+dd(b.weekT)));if(b.vid)it.push(h('span',{title:b.vidTitle||''},'Best video, 1st day ',h('b',{text:cmp(b.vid)+' views'})));
if(it.length)card.append(h('div',{class:'pb'},h('span',{text:'Records:'}),it))}
if(c.rv&&c.rv.length){const box=h('details',{class:'cm'}),tb=h('table',{class:'rvt'});let sk=RVS[c.i]||'pd';
const rows=c.rv.map(r=>{const d=Math.max(1,(Date.now()/1000-r[2])/86400);return{id:r[0],t:r[1],pub:r[2],v:r[3],pd:r[3]/d,lk:r[3]>0&&r[4]>=0?r[4]/r[3]*100:-1}});
const keys=[['v','Views'],['pd','Per day'],['lk','Likes %']];
const draw=()=>{tb.textContent='';tb.append(h('tr',{class:'hd'},h('th',{text:'Video'}),keys.map(([k,t])=>h('th',{class:'n'},h('a',{href:'#',text:t+(sk==k?' ▾':''),onclick:e=>{e.preventDefault();sk=k;RVS[c.i]=k;draw()}})))));
[...rows].sort((a,b)=>b[sk]-a[sk]).forEach(r=>tb.append(h('tr',{},h('td',{class:'nm2'},h('a',{href:'https://youtu.be/'+r.id,target:'_blank',text:r.t,title:r.t})),h('td',{class:'n',text:cmp(r.v)}),h('td',{class:'n',text:cmp(Math.round(r.pd))}),h('td',{class:'n',text:r.lk>=0?r.lk.toFixed(1)+'%':'–'}))))};
draw();box.append(h('summary',{},h('h3',{style:'display:inline',text:'Recent uploads ('+rows.length+')'})),tb);card.append(box)}
if(c.vid&&(c.cm||c.cmOff)){const box=h('div',{class:'cm'},h('h3',{text:'Latest comments'}));
if(c.cmOff)box.append(h('div',{class:'sub',text:'Comments are off on this video'}));
else c.cm.forEach(m=>box.append(h('div',{class:'c'},h('b',{text:m.a}),h('span',{class:'sub',text:'  '+ago(m.ts)+(m.l>0?' · '+cmp(m.l)+' likes':'')}),h('p',{text:m.t}))));card.append(box)}
return card}
const COLS=['#3987e5','#d95926','#199e70','#c98500','#d55181','#008300','#9085e9','#e66767'];
const colFor=i=>i<COLS.length?COLS[i]:'#8d8d95';
let CMP={sel:null,days:7,mode:'gain'},HC={},RVS={};
async function hist(i,days){const k=i+':'+days;if(!HC[k]||Date.now()-HC[k].at>120000){const r=await fetch('/api/history?i='+i+'&days='+days);HC[k]={at:Date.now(),p:await r.json()}}return HC[k].p}
function compareCard(){const C=D.channels.filter(c=>c.subs>=0&&kindOk(c));
if(!CMP.sel){const o=[...C].filter(c=>c.i!=0).sort((a,b)=>(b.d7||0)-(a.d7||0));const base=C.some(c=>c.i==0)?[0]:[];CMP.sel=[...base,...o.slice(0,3-base.length).map(c=>c.i)]}
const card=h('div',{class:'card wide'}),box=h('div',{class:'cmp'}),ctl=h('div',{class:'ctl'});
card.append(h('h2',{text:'Compare'}),h('div',{class:'sub',style:'margin:-8px 0 12px',text:'Subscribers gained over the period, so big and small channels fit on one chart. Pick up to 4.'}),ctl,box,h('div',{style:'display:flex;margin-top:8px'},h('a',{class:'small',href:'/api/csv',text:'Download all history (CSV)'})));
const draw=()=>{ctl.textContent='';
C.forEach(c=>{const on=CMP.sel.includes(c.i);ctl.append(h('button',{class:'tg'+(on?' on':''),onclick:()=>{if(on)CMP.sel=CMP.sel.filter(x=>x!=c.i);else if(CMP.sel.length<4)CMP.sel.push(c.i);draw()}},h('i',{style:'background:'+(on?colFor(c.i):'transparent')+';border:1px solid '+colFor(c.i)}),name(c)))});
const seg=(opts,key)=>h('div',{class:'seg'},opts.map(([v,t])=>h('button',{class:CMP[key]==v?'on':'',text:t,onclick:()=>{CMP[key]=v;draw()}})));
ctl.append(h('span',{style:'flex:1'}),seg([[7,'7 days'],[30,'30 days']],'days'),seg([['gain','Gained'],['pct','% growth']],'mode'));
plot(box,C)};draw();return card}
async function plot(box,C){const sel=CMP.sel.slice(),days=CMP.days,mode=CMP.mode;
const ser=[];for(const i of sel){const c=C.find(x=>x.i==i);if(!c)continue;const p=(await hist(i,days)).slice();p.push([Date.now()/1000|0,c.subs]);if(p.length<2)continue;const b=p[0][1];
ser.push({c,pts:p.map(([t,v])=>[t,mode=='pct'?(b>0?(v-b)/b*100:0):v-b])})}
const BW=box.clientWidth||900;box.textContent='';if(!ser.length){box.append(h('div',{class:'sub',style:'padding:40px 0;text-align:center',text:sel.length?'Not enough history yet':'Pick a channel above'}));return}
const W=Math.max(280,BW),narrow=W<600,H=260,L=narrow?46:56,R=narrow?10:150,T=12,B=28,ns='http://www.w3.org/2000/svg',mk=(t,a)=>{const e=document.createElementNS(ns,t);for(const k in a)e.setAttribute(k,a[k]);return e};
const now=Date.now()/1000,t0=now-days*86400;let mn=0,mx=0;ser.forEach(s=>s.pts.forEach(([,v])=>{mn=Math.min(mn,v);mx=Math.max(mx,v)}));if(mx==mn)mx=mn+1;
const st=(()=>{const r=(mx-mn)/4,m=Math.pow(10,Math.floor(Math.log10(r))),f=r/m;return (f<=1?1:f<=2?2:f<=2.5?2.5:f<=5?5:10)*m})();mn=Math.floor(mn/st)*st;mx=Math.ceil(mx/st)*st;const nT=Math.round((mx-mn)/st);
const X=t=>L+(Math.max(t,t0)-t0)/(now-t0)*(W-L-R),Y=v=>T+(mx-v)/(mx-mn)*(H-T-B);
const fv=v=>mode=='pct'?(v>=0?'+':'')+v.toFixed(Math.abs(mx)<1?2:1)+'%':(v>=0?'+':'')+cmp(Math.round(v));
const svg=mk('svg',{viewBox:`0 0 ${W} ${H}`});
for(let k=0;k<=nT;k++){const v=mn+st*k,y=Y(v);svg.append(mk('line',{x1:L,x2:W-R,y1:y,y2:y,stroke:'#2a2a2e','stroke-width':1}));const tx=mk('text',{x:L-8,y:y+4,'text-anchor':'end','font-size':11,fill:'#8d8d95'});tx.textContent=fv(v);svg.append(tx)}
[0,.5,1].forEach(f=>{const t=t0+(now-t0)*f,tx=mk('text',{x:L+(W-L-R)*f,y:H-8,'text-anchor':f==0?'start':f==1?'end':'middle','font-size':11,fill:'#8d8d95'});tx.textContent=f==1?'now':new Date(t*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'});svg.append(tx)});
const ends=[];ser.forEach(s=>{const d=s.pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
svg.append(mk('path',{d,fill:'none',stroke:colFor(s.c.i),'stroke-width':2,'stroke-linejoin':'round','vector-effect':'non-scaling-stroke','stroke-dasharray':s.c.i<COLS.length?'':'5 4'}));
const last=s.pts[s.pts.length-1];ends.push({s,y:Y(last[1]),v:last[1]})});
ends.sort((a,b)=>a.y-b.y);for(let k=1;k<ends.length;k++)if(ends[k].y-ends[k-1].y<16)ends[k].y=ends[k-1].y+16;
ends.forEach(e=>{svg.append(mk('circle',{cx:W-R,cy:Y(e.s.pts[e.s.pts.length-1][1]),r:4,fill:colFor(e.s.c.i),stroke:'#18181b','stroke-width':2}));if(narrow)return;const g=mk('text',{x:W-R+8,y:e.y+4,'font-size':12,fill:'#f2f2f3'});g.textContent=name(e.s.c).slice(0,14)+' '+fv(e.v);svg.append(g)});
const cross=mk('line',{y1:T,y2:H-B,stroke:'#8d8d95','stroke-width':1,visibility:'hidden'});svg.append(cross);
const tip=h('div',{class:'tip'});box.append(svg,tip);
svg.onmousemove=ev=>{const r=svg.getBoundingClientRect(),sx=(ev.clientX-r.left)/r.width*W;if(sx<L||sx>W-R){svg.onmouseleave();return}
const t=t0+(sx-L)/(W-L-R)*(now-t0);cross.setAttribute('x1',sx);cross.setAttribute('x2',sx);cross.setAttribute('visibility','visible');
tip.textContent='';tip.append(h('div',{class:'sub',text:new Date(t*1000).toLocaleString('en-GB',{day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'})}));
ser.forEach(s=>{let v=s.pts[0][1];for(const p of s.pts){if(p[0]>t)break;v=p[1]}tip.append(h('div',{},h('i',{style:'background:'+colFor(s.c.i)}),name(s.c)+' ',h('b',{text:fv(v)})))});
tip.style.display='block';const px=ev.clientX-r.left;tip.style.left=Math.min(px+12,r.width-tip.offsetWidth-4)+'px';tip.style.top='10px'};
svg.onmouseleave=()=>{cross.setAttribute('visibility','hidden');tip.style.display='none'}}
let PF='all';try{PF=localStorage.getItem('pf')||'all'}catch(e){}
const PIC={yt:'<svg width="18" height="13" viewBox="0 0 20 14"><rect width="20" height="14" rx="4" fill="#ff0033"/><path d="M8 4v6l5-3z" fill="#fff"/></svg>',tw:'<svg width="15" height="15" viewBox="0 0 16 16"><path d="M2 1h13v9l-4 4H8l-2 2H4v-2H1V4z" fill="#9146ff"/><path d="M7 4h1.5v4H7zM10.5 4H12v4h-1.5z" fill="#fff"/></svg>'};
function pIcon(c){const s=document.createElement('span');s.className='pi';s.title=c.tw?'Twitch':'YouTube';s.innerHTML=PIC[c.tw?'tw':'yt'];return s}
const kindOk=c=>PF=='all'||(PF=='tw'?!!c.tw:!c.tw);
function filterBar(){const all=D.channels,hasTw=all.some(c=>c.tw),hasYt=all.some(c=>!c.tw);const bar=$('#pf');bar.textContent='';
if(!(hasTw&&hasYt)){bar.style.display='none';if(PF!='all')PF='all';return}bar.style.display='';
[['all','All',all.length],['yt','YouTube',all.filter(c=>!c.tw).length],['tw','Twitch',all.filter(c=>c.tw).length]].forEach(([k,t,n])=>bar.append(h('button',{class:PF==k?'on':'',onclick:()=>{PF=k;try{localStorage.setItem('pf',k)}catch(e){}CMP.sel=null;render()}},t+' ',h('span',{text:n}))))}
function render(){const app=$('#app');app.textContent='';filterBar();const C=D.channels.filter(kindOk);if(D.accent)document.documentElement.style.setProperty('--gold',D.accent);
$('#meta').textContent=(D.err?D.err:(D.updatedAgo>=0?'Updated '+(D.updatedAgo<60?'just now':(D.updatedAgo/60|0)+' min ago'):''))+(D.ver?'  ·  v'+D.ver:'');{const u=$('#upd');if(D.upd){u.textContent='Update v'+D.upd;u.style.display=''}else u.style.display='none'}
const row=h('aside',{class:'side'});
// summary
const ok=C.filter(c=>c.statsOk).sort((a,b)=>b.d1-a.d1);const nv=C.filter(c=>c.vid&&!c.tw&&Date.now()/1000-c.vid.pub<86400);
row.append(h('div',{class:'card'},h('h2',{text:'Last 24 hours'}),h('div',{class:'sub',style:'margin:-8px 0 8px',text:'Rolling: gains since this time yesterday'}),h('div',{class:'list'},ok.length?ok.slice(0,5).map(c=>h('div',{},h('span',{text:name(c)}),h('b',{class:cls(c.d1),text:sg(c.d1)}))):h('div',{class:'sub',text:'Collecting data – check back in a few hours'})),h('div',{class:'sub',style:'margin-top:8px',text:nv.length?`${nv.length} new video${nv.length>1?'s':''} today`:'No new videos in the last day'})));
// weather
if(D.wx){const w=D.wx;row.append(h('div',{class:'card'},h('h2',{text:'Weather · '+w.place}),h('div',{class:'big',text:w.temp+'°'}),h('div',{text:w.text+' · feels '+w.feels+'°'}),h('div',{class:'sub',text:`High ${w.hi}° · Low ${w.lo}° · Wind ${w.wind} mph`}),w.rainHour>=0?h('div',{style:'margin-top:8px;color:var(--blue)',text:`Rain likely around ${String(w.rainHour).padStart(2,'0')}:00 (${w.rainPct}%)`}):null))}
// race
if(D.race&&kindOk(D.channels[D.race[0]])&&kindOk(D.channels[D.race[1]])){const A=D.channels[D.race[0]],B=D.channels[D.race[1]],ea=est(A),eb=est(B),fa=ea+eb?ea/(ea+eb):.5;const lead=ea>=eb?A:B,ch=lead===A?B:A,closing=(ch.rate||0)-(lead.rate||0),gap=Math.abs(ea-eb);
row.append(h('div',{class:'card race'},h('h2',{text:'Race'}),h('div',{class:'rs'},h('span',{},av(A),pIcon(A),h('b',{style:'color:var(--red)',text:name(A)})),h('b',{text:fmt(ea)})),h('div',{class:'rs'},h('span',{},av(B),pIcon(B),h('b',{style:'color:var(--blue)',text:name(B)})),h('b',{text:fmt(eb)})),h('div',{class:'tug'},h('div',{class:'a',style:`width:${fa*100}%`}),h('div',{class:'b',style:`width:${(1-fa)*100}%`})),h('div',{text:`Gap ${fmt(gap)}`}),h('div',{class:'sub',text:closing>0.01?`${name(ch)} is catching up by ${fmt(Math.round(closing))}/day – could pass in about ${Math.max(1,Math.round(gap/closing))} days`:(lead.rate||ch.rate)?`${name(lead)} is pulling away`:'Trend: need more data'})))}
// leaderboard
const lb=[...C].filter(c=>c.subs>=0).sort((a,b)=>b.subs-a.subs);const mixed=C.some(c=>c.tw)&&C.some(c=>!c.tw);
row.append(h('div',{class:'card'},h('h2',{text:'Leaderboard'}),h('table',{},h('tr',{class:'hd'},h('th',{colspan:'3'}),h('th',{class:'n',text:C.some(c=>c.tw)?'Total':'Subs'}),h('th',{class:'n',text:'Today',title:'Since midnight'})),lb.map((c,k)=>h('tr',{class:c.i==0?'me':''},h('td',{text:k+1+'.'}),h('td',{},av(c)),h('td',{class:'nm2'},mixed?pIcon(c):null,name(c)),h('td',{class:'n',text:cmp(c.subs)}),h('td',{class:'n '+cls(c.today),text:c.statsOk?sg(c.today):''}))))));
{const mon=t=>new Date(t*1000).toLocaleDateString('en-GB',{month:'short'});const ms=[...C].filter(c=>c.subs>=0).sort((a,b)=>(b.mo||0)-(a.mo||0));
if(ms.length&&C.some(c=>c.statsOk))row.append(h('div',{class:'card'},h('h2',{text:'Monthly'}),h('table',{},h('tr',{class:'hd'},h('th',{}),h('th',{class:'n',text:mon(D.m1)}),h('th',{class:'n',text:mon(D.m0)+' so far'})),ms.map(c=>h('tr',{class:c.i==0?'me':''},h('td',{class:'nm2',text:name(c)}),h('td',{class:'n '+(c.lm!=null?cls(c.lm):''),text:c.lm!=null?sg(c.lm):'–'}),h('td',{class:'n '+cls(c.mo),text:c.statsOk?sg(c.mo):'–'}))))))}
const grid=h('section',{class:'grid'});C.forEach(c=>grid.append(channelCard(c)));if(C.filter(c=>c.subs>=0).length>1)grid.append(compareCard());app.append(h('div',{class:'layout'},row,grid))}
async function load(){try{const r=await fetch('/api/data');D=await r.json();render()}catch(e){$('#meta').textContent='Board not reachable'}}
setInterval(()=>{if(!D)return;document.querySelectorAll('[data-est]').forEach(el=>{const c=D.channels[el.dataset.est];el.textContent=fmt(est(c))})},1000);
load();setInterval(load,60000);
</script></body></html>)HTML";

void handleDashboard() {
  if (portalMode) { handleSettings(); return; }
  server.send_P(200, "text/html", DASH_HTML);
}

void handleApiData() {
  JsonDocument doc;
  doc["now"] = (long)nowT();
  doc["updatedAgo"] = lastFetchOk ? (long)((millis() - lastFetchOk) / 1000) : -1;
  doc["err"] = netError;
  doc["est"] = cfgEst;
  doc["ip"] = WiFi.localIP().toString();
  doc["ver"] = FW_VERSION;
  doc["accent"] = THEMES[cfgTheme].css;
  if (updAvail) doc["upd"] = updVer;
  doc["m0"] = (long)monthStart(0); doc["m1"] = (long)monthStart(1);
  if (raceSet()) { JsonArray r = doc["race"].to<JsonArray>(); r.add(cfgRaceA); r.add(cfgRaceB); }
  else doc["race"] = nullptr;
  if (wx.ok) {
    JsonObject w = doc["wx"].to<JsonObject>();
    w["place"] = cfgWxName; w["temp"] = lroundf(wx.temp); w["feels"] = lroundf(wx.feels); w["text"] = wxText(wx.code);
    w["hi"] = wx.hi; w["lo"] = wx.lo; w["rainHour"] = wx.rainHour; w["rainPct"] = wx.rainPct; w["wind"] = lroundf(wx.wind);
  }
  JsonArray arr = doc["channels"].to<JsonArray>();
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    JsonObject o = arr.add<JsonObject>();
    o["i"] = i; o["handle"] = c.handle; o["id"] = c.id; o["title"] = c.title;
    o["subs"] = c.subs; o["step"] = (c.subs >= 0 && !c.tw) ? stepFor(c.subs) : 1;
    o["rate"] = c.ratePerDay; o["stepAt"] = (long)c.stepChangedAt;
    o["views"] = c.views; o["videos"] = c.videos; o["joined"] = (long)c.joined;
    o["country"] = c.country; o["avatar"] = c.avatarUrl; o["err"] = c.err;
    o["statsOk"] = c.statsOk; o["today"] = c.gainToday; o["d1"] = c.gain24;
    o["d7"] = c.gain7; o["d30"] = c.gain30; o["histStart"] = (long)c.histStart;
    long e = c.subs >= 0 ? estimateFor(i) : 0;
    long nx = nextMilestone(max(0L, e));
    o["next"] = nx; o["prev"] = prevMilestone(nx);
    if (i == 0 && vt.active && vt.n) {
      JsonObject t = o["vt"].to<JsonObject>();
      t["views"] = vtViews(); t["ageMin"] = vtAgeMin(); t["rate"] = vtRate(); t["typical"] = vt.typical;
      t["at1h"] = vt.at1h; t["base1h"] = vt.base1h; t["at24h"] = vt.at24h; t["base24h"] = vt.base24h; t["cmp"] = vtCompare();
      JsonArray pts = t["pts"].to<JsonArray>();
      for (int k = 0; k < vt.n; k++) { JsonArray pp = pts.add<JsonArray>(); pp.add(vt.t[k]); pp.add(vt.v[k]); }
    }
    if (c.tw) {
      o["tw"] = c.twLogin;
      if (c.live) { JsonObject v = o["vid"].to<JsonObject>(); v["id"] = ""; v["title"] = c.vidTitle; v["pub"] = (long)c.vidPublished;
                    v["live"] = true; v["viewers"] = c.liveViewers; v["thumb"] = c.twThumb; v["game"] = c.twGame; }
    }
    o["mo"] = c.gainMonth;
    if (c.lastMonthOk) o["lm"] = c.gainLastMonth;
    if (!c.pbLoaded) pbLoad(i);
    if (c.pbDay || c.pbWeek || c.pbVid) {
      JsonObject b = o["pb"].to<JsonObject>();
      b["day"] = c.pbDay; b["dayT"] = c.pbDayT; b["week"] = c.pbWeek; b["weekT"] = c.pbWeekT;
      if (c.pbVid) { b["vid"] = c.pbVid; b["vidT"] = c.pbVidT; b["vidTitle"] = c.pbVidTitle; }
    }
    if (c.cmOff) o["cmOff"] = true;
    if (c.rvN) {
      JsonArray rv = o["rv"].to<JsonArray>();
      for (int k = 0; k < c.rvN; k++) {
        JsonArray x = rv.add<JsonArray>();
        x.add(c.rv[k].id); x.add(c.rv[k].title); x.add((long)c.rv[k].pub); x.add(c.rv[k].views); x.add(c.rv[k].likes); x.add(c.rv[k].comments);
      }
    }
    if (c.cmN) {
      JsonArray cm = o["cm"].to<JsonArray>();
      for (int k = 0; k < c.cmN; k++) {
        JsonObject x = cm.add<JsonObject>();
        x["a"] = c.cmAuthor[k]; x["t"] = c.cmText[k]; x["ts"] = (long)c.cmT[k]; x["l"] = c.cmLikes[k];
      }
    }
    if (!c.tw && c.vidId.length() && c.vidTitle.length()) {
      JsonObject v = o["vid"].to<JsonObject>();
      v["id"] = c.vidId; v["title"] = c.vidTitle; v["pub"] = (long)c.vidPublished; v["dur"] = c.vidDuration;
      v["views"] = c.vidViews; v["likes"] = c.vidLikes; v["comments"] = c.vidComments;
      v["live"] = c.live; v["viewers"] = c.liveViewers;
    }
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// [[time, subs], ...] for one channel, streamed in chunks
void handleApiHistory() {
  int i = server.arg("i").toInt();
  int days = constrain(server.arg("days").toInt(), 1, 31);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  if (i >= 0 && i < numCh && fsOk && ch[i].id.length()) {
    File f = LittleFS.open(histPath(i), "r");
    if (f) {
      time_t from = nowT() - (time_t)days * 86400;
      String chunk; bool first = true; Sample s;
      while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
        if ((time_t)s.t < from) continue;
        chunk += (first ? "[" : ",[") + String(s.t) + "," + String(s.s) + "]";
        first = false;
        if (chunk.length() > 1200) { server.sendContent(chunk); chunk = ""; }
      }
      f.close();
      if (chunk.length()) server.sendContent(chunk);
    }
  }
  server.sendContent("]");
  server.sendContent("");
}

// History as CSV: /api/csv?i=2 for one channel, /api/csv for all of them
void handleApiCsv() {
  int only = server.hasArg("i") ? server.arg("i").toInt() : -1;
  String fname = only >= 0 && only < numCh ? nameOf(only) : String("all-channels");
  String safe; for (char ch2 : fname) safe += isalnum((unsigned char)ch2) ? ch2 : '-';
  server.sendHeader("Content-Disposition", "attachment; filename=\"subcounter-" + safe + ".csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent("channel,time_utc,subscribers\n");
  for (int i = 0; i < numCh; i++) {
    if (only >= 0 && i != only) continue;
    if (!fsOk || !ch[i].id.length()) continue;
    File f = LittleFS.open(histPath(i), "r");
    if (!f) continue;
    String nm = nameOf(i); nm.replace("\"", "'");
    String chunk; Sample sm;
    while (f.read((uint8_t *)&sm, sizeof(sm)) == sizeof(sm)) {
      char ts[24]; time_t t = sm.t; struct tm g; gmtime_r(&t, &g);
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &g);
      chunk += "\"" + nm + "\"," + ts + "," + String(sm.s) + "\n";
      if (chunk.length() > 1200) { server.sendContent(chunk); chunk = ""; }
    }
    f.close();
    if (chunk.length()) server.sendContent(chunk);
  }
  server.sendContent("");
}

// ── Settings backup / restore ───────────────────────────────────────────────
// Every saved setting (NVS namespace "subcounter") as JSON. Passwords, keys and
// sign-ins are left out unless asked for. History and records aren't included.
bool secretKey(const char *k) {
  String s = k;
  return s == "apikey" || s == "pin" || s == "spsec" || s == "spref" || s == "twsec" || s.endsWith("pass");
}
void handleNtfyTest() {
  if (!authed()) return;
  String t = server.arg("ntfy"); t.trim(); t.replace(" ", "-");
  if (!t.length()) { sendMessage(400, "No topic", "Type an ntfy topic first."); return; }
  cfgNtfy = t;                          // use what's typed (saved properly when you press Save)
  queueNote("SubCounter test", "Notifications are working! v" FW_VERSION, "white_check_mark", "http://" + WiFi.localIP().toString() + "/");
  sendMessage(200, "Test sent", "Check your phone in a few seconds. If nothing arrives, make sure the ntfy app is subscribed to <b>" + htmlEscape(t) + "</b>. Don't forget to press Save.");
}
void handleBackup() {
  if (!authed()) return;
  bool secrets = server.arg("secrets") == "1";
  JsonDocument doc;
  doc["subcounter_backup"] = 1; doc["firmware"] = FW_VERSION; doc["with_secrets"] = secrets;
  JsonObject keys = doc["keys"].to<JsonObject>();
  prefs.begin("subcounter", true);
  nvs_iterator_t it = nullptr;
  esp_err_t r = nvs_entry_find("nvs", "subcounter", NVS_TYPE_ANY, &it);
  while (r == ESP_OK) {
    nvs_entry_info_t info; nvs_entry_info(it, &info);
    if (secrets || !secretKey(info.key)) {
      JsonObject o = keys[info.key].to<JsonObject>();
      switch (info.type) {
        case NVS_TYPE_U8:  o["t"] = "u8";  o["v"] = prefs.getUChar(info.key); break;
        case NVS_TYPE_I8:  o["t"] = "i8";  o["v"] = prefs.getChar(info.key); break;
        case NVS_TYPE_U16: o["t"] = "u16"; o["v"] = prefs.getUShort(info.key); break;
        case NVS_TYPE_I16: o["t"] = "i16"; o["v"] = prefs.getShort(info.key); break;
        case NVS_TYPE_U32: o["t"] = "u32"; o["v"] = prefs.getUInt(info.key); break;
        case NVS_TYPE_I32: o["t"] = "i32"; o["v"] = prefs.getInt(info.key); break;
        case NVS_TYPE_STR: o["t"] = "str"; o["v"] = prefs.getString(info.key); break;
        case NVS_TYPE_BLOB: {
          size_t n = prefs.getBytesLength(info.key); uint8_t b[64];
          if (n <= sizeof(b)) { prefs.getBytes(info.key, b, n); String hx; char t[3];
            for (size_t k = 0; k < n; k++) { snprintf(t, 3, "%02x", b[k]); hx += t; }
            o["t"] = "blob"; o["v"] = hx; }
          break; }
        default: keys.remove(info.key); break;
      }
    }
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  prefs.end();
  String out; serializeJsonPretty(doc, out);
  server.sendHeader("Content-Disposition", String("attachment; filename=\"subcounter-settings") + (secrets ? "-with-keys" : "") + ".json\"");
  server.send(200, "application/json", out);
}
void handleRestore() {
  if (!authed()) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !(doc["subcounter_backup"] | 0)) {
    server.send(400, "text/plain", "That isn't a SubCounter settings file."); return;
  }
  int n = 0;
  prefs.begin("subcounter", false);
  for (JsonPair kv : doc["keys"].as<JsonObject>()) {
    const char *k = kv.key().c_str(); String t = kv.value()["t"] | "";
    JsonVariant v = kv.value()["v"];
    if (strlen(k) > 15) continue;
    if (t == "u8") prefs.putUChar(k, v.as<uint8_t>());
    else if (t == "i8") prefs.putChar(k, v.as<int8_t>());
    else if (t == "u16") prefs.putUShort(k, v.as<uint16_t>());
    else if (t == "i16") prefs.putShort(k, v.as<int16_t>());
    else if (t == "u32") prefs.putUInt(k, v.as<uint32_t>());
    else if (t == "i32") prefs.putInt(k, v.as<int32_t>());
    else if (t == "str") prefs.putString(k, v.as<String>());
    else if (t == "blob") { String hx = v.as<String>(); uint8_t b[64]; size_t m = min((size_t)64, hx.length() / 2);
      for (size_t i = 0; i < m; i++) b[i] = strtoul(hx.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
      prefs.putBytes(k, b, m); }
    else continue;
    n++;
  }
  prefs.end();
  server.send(200, "text/plain", "Restored " + String(n) + " settings. Restarting...");
  delay(800);
  ESP.restart();
}

const char SCAN_JS[] PROGMEM = R"JS(<style>
.net{display:flex;align-items:center;gap:10px;width:100%;margin:0;padding:11px 12px;border:1px solid #333;border-radius:10px;background:#111;color:#fff;font-size:15px;font-weight:400;text-align:left;cursor:pointer;margin-top:6px}
.net:hover{border-color:#e62117}.net b{flex:1;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.net small{margin:0;color:#888}.bars{display:inline-flex;gap:2px;align-items:flex-end;height:14px}.bars i{width:4px;background:#555;border-radius:1px}.bars i.on{background:#30d158}
</style><script>
async function scan(){
  const box=document.getElementById('nets'),btn=document.getElementById('scanBtn');
  btn.disabled=true;btn.textContent='Scanning… (a few seconds)';box.textContent='';
  try{
    const r=await fetch('/api/scan');const list=await r.json();
    if(!list.length){box.innerHTML='<small>No networks found. Is the hotspot on? (iPhone: keep the Personal Hotspot screen open and turn on Maximise Compatibility)</small>'}
    list.forEach(n=>{
      const b=document.createElement('button');b.type='button';b.className='net';
      const bars=document.createElement('span');bars.className='bars';
      const lvl=n.rssi>-55?4:n.rssi>-67?3:n.rssi>-78?2:1;
      for(let k=1;k<=4;k++){const i=document.createElement('i');i.style.height=(k*3+2)+'px';if(k<=lvl)i.className='on';bars.append(i)}
      const nm=document.createElement('b');nm.textContent=n.ssid;
      const info=document.createElement('small');
      info.textContent=(n.saved?'saved · ':'')+(n.ent?'needs username':(n.open?'open':'🔒'));
      b.append(bars,nm,info);
      b.onclick=()=>{document.getElementById('s').value=n.ssid;document.querySelectorAll('.net').forEach(x=>x.style.borderColor='');b.style.borderColor='#30d158';
        const pw=document.getElementById('pw');pw.value='';pw.focus();pw.scrollIntoView({block:'center',behavior:'smooth'})};
      box.append(b)});
  }catch(e){box.innerHTML='<small>Scan failed – try again.</small>'}
  btn.disabled=false;btn.textContent='\u{1F4F6} Scan again';
}
</script>)JS";

// Nearby networks for the settings page: [{ssid, rssi, open, ent, saved}, ...] strongest first
void handleApiScan() {
  if (!authed()) return;
  int n = WiFi.scanNetworks();
  struct Found { String ssid; int rssi; wifi_auth_mode_t auth; } f[25]; int cnt = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;                                  // hidden networks
    int dup = -1;
    for (int k = 0; k < cnt; k++) if (f[k].ssid == s) dup = k;
    if (dup >= 0) { if (WiFi.RSSI(i) > f[dup].rssi) f[dup].rssi = WiFi.RSSI(i); continue; }
    if (cnt < 25) { f[cnt].ssid = s; f[cnt].rssi = WiFi.RSSI(i); f[cnt].auth = WiFi.encryptionType(i); cnt++; }
  }
  WiFi.scanDelete();
  for (int a = 0; a < cnt; a++) for (int b = a + 1; b < cnt; b++)      // strongest first
    if (f[b].rssi > f[a].rssi) { Found t = f[a]; f[a] = f[b]; f[b] = t; }
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int k = 0; k < cnt; k++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = f[k].ssid;
    o["rssi"] = f[k].rssi;
    o["open"] = (f[k].auth == WIFI_AUTH_OPEN);
    o["ent"] = (f[k].auth == WIFI_AUTH_WPA2_ENTERPRISE || f[k].auth == WIFI_AUTH_WPA3_ENTERPRISE || f[k].auth == WIFI_AUTH_WPA2_WPA3_ENTERPRISE);
    bool saved = false;
    for (int j = 0; j < numNets; j++) if (nets[j].ssid == f[k].ssid) saved = true;
    o["saved"] = saved;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

String checkbox(const char *name, bool on, const char *label) {
  return String("<label><input type='checkbox' name='") + name + "' value='1' style='width:auto'" + (on ? " checked" : "") + "> " + label + "</label>";
}

String hourSelect(const char *name, int val) {
  String s = String("<select name='") + name + "'><option value='-1'" + (val < 0 ? " selected" : "") + ">Off</option>";
  for (int h = 0; h < 24; h++) {
    char b[8]; snprintf(b, sizeof(b), "%02d:00", h);
    s += "<option value='" + String(h) + "'" + (val == h ? " selected" : "") + ">" + b + "</option>";
  }
  return s + "</select>";
}

// Race pickers: YouTube and Twitch channels in their own groups when you follow both
String channelSelect(const char *name, int val) {
  String s = String("<select name='") + name + "' onchange='racePv()'><option value='-1'>None</option>";
  bool hasTw = false, hasYt = false;
  for (int i = 0; i < numCh; i++) { if (ch[i].tw) hasTw = true; else hasYt = true; }
  for (int g = 0; g < 2; g++) {
    bool tw = g == 1;
    if ((tw && !hasTw) || (!tw && !hasYt)) continue;
    if (hasTw && hasYt) s += String("<optgroup label='") + (tw ? "Twitch" : "YouTube") + "'>";
    for (int i = 0; i < numCh; i++) {
      if (ch[i].tw != tw) continue;
      s += "<option value='" + String(i) + "' data-p='" + (tw ? "tw" : "yt") + "'" + (val == i ? " selected" : "") + ">" +
           htmlEscape(ch[i].title.length() ? ch[i].title : ch[i].handle) + "</option>";
    }
    if (hasTw && hasYt) s += "</optgroup>";
  }
  return s + "</select>";
}

// Section heading with an anchor for the jump links at the top
String sec(const char *id, const char *title) {
  return String("<h2 id='") + id + "'>" + title + "</h2>";
}

void handleSettings() {
  if (!authed()) return;
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>&#9654; SubCounter settings</h1><p>Saved on the board only. v" FW_VERSION;
  if (!portalMode) h += " &middot; <a href='/'>&larr; Dashboard</a>";
  h += "</p><nav class='jump'><a href='#channels'>YouTube</a><a href='#twitch'>Twitch</a><a href='#display'>Display</a><a href='#alerts'>Alerts</a>"
       "<a href='#phone'>Phone</a><a href='#weather'>Weather</a><a href='#spotify'>Spotify</a><a href='#motion'>Motion</a><a href='#wifi'>Wi-Fi</a>"
       "<a href='#updates'>Updates</a><a href='#security'>Security</a>" + String(portalMode ? "" : "<a href='#firmware'>Firmware</a>") + "</nav>";
  if (wifiFailReason.length()) {
    h += "<div style='background:#3a1210;border:1px solid #e62117;border-radius:10px;padding:12px;margin-bottom:8px;font-size:14px'>"
         "<b>Couldn't connect to Wi-Fi:</b><br>" + htmlEscape(wifiFailReason) + "</div>";
  }
  // Everything is inside ONE form so a single Save sends it all. Extra actions
  // (Connect now, Spotify, calibrate) are buttons with their own formaction.
  h += "<form method='POST' action='/save'>";

  // ── YouTube ──
  h += sec("channels", "YouTube");
  h += "<label>YouTube channels (one per line)</label>";
  h += "<textarea name='channels' autocapitalize='off' autocorrect='off' spellcheck='false' placeholder='@yourchannel&#10;@mkbhd&#10;@veritasium'>" +
       htmlEscape(cfgChannels) + "</textarea>";
  h += "<small>@handles, UC… channel IDs or channel links. Put your own channel first – it's highlighted and shown on the night clock.</small>";
  h += "<label>YouTube Data API key</label>";
  h += "<input name='apikey' autocapitalize='off' autocomplete='off' placeholder='";
  h += cfgApiKey.length() ? "(saved — leave blank to keep)" : "AIza…";
  h += "'><small>Only needed if you follow YouTube channels.</small>";

  // ── Twitch ──
  h += sec("twitch", "Twitch");
  h += "<label>Twitch channels (just the name, one per line)</label>";
  h += "<textarea name='twch' autocapitalize='off' autocorrect='off' spellcheck='false' style='min-height:90px' placeholder='shroud&#10;pokimane'>" +
       htmlEscape(cfgTwitch) + "</textarea>";
  h += "<small>Type the name as it appears in their Twitch address (twitch.tv/<b>name</b>). Pasting the full link works too. "
       "YouTube and Twitch together: up to 10 channels.</small>";
  if (twError.length() && !portalMode) h += "<small style='color:#e62117'>" + htmlEscape(twError) + "</small>";
  h += "<details" + String(cfgTwId.length() && cfgTwSecret.length() ? "" : " open") + "><summary>Twitch app details" +
       String(cfgTwId.length() && cfgTwSecret.length() ? " (saved &#10003;)" : " (needed for Twitch channels)") + "</summary>";
  h += "<small>Create a free app at <a href='https://dev.twitch.tv/console/apps' target='_blank'>dev.twitch.tv/console</a>: "
       "Register Your Application &rarr; any unique name &rarr; OAuth Redirect URL <code>https://localhost</code> (any https address works, it isn't used) "
       "&rarr; Category: Other &rarr; Client type: Confidential &rarr; Create &rarr; Manage &rarr; copy the Client ID and a <b>New Secret</b>. "
       "Your Twitch account needs two-factor sign-in turned on.</small>";
  h += "<label>Client ID</label><input name='twid' autocapitalize='off' autocomplete='off' value='" + htmlEscape(cfgTwId) + "'>";
  h += "<label>Client Secret</label><input name='twsec' type='text' class='secret' autocapitalize='off' autocomplete='off' spellcheck='false' placeholder='";
  h += cfgTwSecret.length() ? "(saved — leave blank to keep)" : "";
  h += "'>";
  h += "</details>";

  // ── Display ──
  h += sec("display", "Display");
  h += "<label>Colour theme</label><select name='theme'>";
  for (int t = 0; t < NUM_THEMES; t++) h += "<option value='" + String(t) + "'" + (cfgTheme == t ? " selected" : "") + ">" + THEMES[t].name + "</option>";
  h += "</select><small>Accent colour on the board and the dashboard.</small>";
  h += "<label>Screen brightness: <b id='bv'>" + String(cfgBright) + "%</b></label>"
       "<input type='range' name='bright' min='10' max='100' step='5' value='" + String(cfgBright) + "' oninput=\"bv.textContent=this.value+'%'\" style='padding:0'>";
  h += "<label>Night clock brightness: <b id='nv'>" + String(cfgNightBright) + "%</b></label>"
       "<input type='range' name='nbright' min='1' max='40' value='" + String(cfgNightBright) + "' oninput=\"nv.textContent=this.value+'%'\" style='padding:0'>";
  h += checkbox("auto", cfgAuto, "Switch channels automatically every 10 seconds");
  h += checkbox("est", cfgEst, "Estimated live counts between YouTube's rounded steps (“est.”)");
  h += "<label>Show the big clock after this long without use</label><select name='idleclk'>";
  { const int opts[] = { 0, 1, 2, 3, 5, 10 };
    for (int o : opts) h += "<option value='" + String(o) + "'" + (cfgIdleClock == o ? " selected" : "") + ">" +
                            (o == 0 ? String("Never") : String(o) + (o == 1 ? " minute" : " minutes")) + "</option>"; }
  h += "</select><small>Touch, press BOOT or pick the board up to go back.</small>";
  h += "<label>Night clock (dims and shows the time)</label><div class='row'>" +
       hourSelect("nightStart", cfgNightStart) + "<span>to</span>" + hourSelect("nightEnd", cfgNightEnd) + "</div>";
  if (numCh >= 2) {
    h += "<label>Subscriber race</label><div class='row'>" + channelSelect("raceA", cfgRaceA) +
         "<span>vs</span>" + channelSelect("raceB", cfgRaceB) + "</div>";
    h += "<div id='rpv' class='rpv'></div><script>"
         "const PI={yt:\"<svg width='20' height='14' viewBox='0 0 20 14'><rect width='20' height='14' rx='4' fill='#ff0033'/><path d='M8 4v6l5-3z' fill='#fff'/></svg>\",tw:\"<svg width='16' height='16' viewBox='0 0 16 16'><path d='M2 1h13v9l-4 4H8l-2 2H4v-2H1V4z' fill='#9146ff'/><path d='M7 4h1.5v4H7zM10.5 4H12v4h-1.5z' fill='#fff'/></svg>\"};"
         "function racePv(){const f=n=>{const o=document.querySelector(\"select[name=\"+n+\"]\").selectedOptions[0];"
         "return o&&o.value!='-1'?'<span>'+PI[o.dataset.p]+' '+o.textContent.replace(/</g,'&lt;')+'</span>':'<span style=\\'color:#777\\'>None</span>'};"
         "document.getElementById('rpv').innerHTML=f('raceA')+'<em>vs</em>'+f('raceB')}racePv()</script>";
    h += "<small>Swipe down twice from the main count to see it.</small>";
  }

  // ── Alerts ──
  h += sec("alerts", "Alerts");
  h += checkbox("celebrate", cfgCelebrate, "Confetti for milestones (bigger milestones, bigger party)");
  h += checkbox("livealert", cfgLiveAlert, "Alert when a channel goes live (and switch to it)");
  h += checkbox("summary", cfgSummary, "Daily summary at 9 am (a monthly recap on the 1st)");
  h += "<small>New-subscriber, overtake and record alerts are always on. Alerts are skipped at night, "
       "when the board is face-down, and while Spotify is playing.</small>";

  // ── Phone notifications ──
  h += sec("phone", "Phone notifications");
  h += "<small>Get alerts on your phone with the free <b>ntfy</b> app (iPhone and Android, no account needed). "
       "Install it, tap <b>+</b>, subscribe to the topic below, then save.</small>";
  { String sug = "subcounter-"; for (int k = 0; k < 6; k++) sug += (char)('a' + esp_random() % 26);
    h += "<label>ntfy topic (blank = off)</label><input name='ntfy' autocapitalize='off' autocomplete='off' spellcheck='false' value='" +
         htmlEscape(cfgNtfy) + "' placeholder='e.g. " + sug + "'>";
    h += "<small>Anyone who knows the topic name can read it, so make it hard to guess.</small>"; }
  const char *nk[] = { "Milestones", "Channel goes live", "New records (your channel)", "Overtakes in the race", "Your new video's view milestones", "Every new subscriber (your channel)" };
  for (int k = 0; k < 6; k++) {
    String nmk = "nt" + String(k);
    h += "<label><input type='checkbox' name='" + nmk + "' value='1' style='width:auto'" + String((cfgNtfyMask >> k) & 1 ? " checked" : "") + "> " + nk[k] + "</label>";
  }
  h += "<details><summary>Own ntfy server (advanced)</summary><label>Server address</label><input name='ntfysrv' autocapitalize='off' value='" + htmlEscape(cfgNtfyServer) + "'></details>";
  if (!portalMode) h += "<button type='submit' class='b2' formaction='/ntfy/test' formnovalidate>Send a test notification</button>";

  // ── Weather ──
  h += sec("weather", "Weather");
  h += "<label>Town or city</label><input name='wxtown' value='" + htmlEscape(cfgWxName) + "' placeholder='e.g. Glasgow'>";
  h += "<small>Long-press the board's screen and pick Weather. Forecasts from Open-Meteo.</small>";

  // ── Spotify ──
  h += sec("spotify", "Spotify");
  if (cfgSpRefresh.length()) h += "<p class='ok'>Connected &#10003;</p>";
  h += "<details" + String(cfgSpId.length() ? "" : " open") + "><summary>Spotify app details (one-time setup, needs Premium)</summary>";
  h += "<small>Put the relay page on GitHub Pages (or any https address you own), then at "
       "<b>developer.spotify.com/dashboard</b> &rarr; Create app &rarr; add that https address as the Redirect URI &rarr; tick <b>Web API</b> &rarr; Save. "
       "Copy the Client ID, Client secret and the same Redirect URI here, and save.</small>";
  h += "<label>Redirect URI (exactly as entered at Spotify)</label><input name='spredir' value='" + htmlEscape(cfgSpRedirect) +
       "' autocapitalize='off' placeholder='https://yourname.github.io/spotify-callback/'>";
  h += "<label>Client ID</label><input name='spid' value='" + htmlEscape(cfgSpId) + "' autocapitalize='off' autocomplete='off'>";
  h += "<label>Client secret</label><input name='spsecret' type='text' class='secret' autocapitalize='off' autocomplete='off' spellcheck='false' placeholder='" +
       String(cfgSpSecret.length() ? "(saved — leave blank to keep)" : "") + "'>";
  h += "</details>";
  if (!portalMode && cfgSpId.length() && cfgSpSecret.length() && cfgSpRedirect.length()) {
    if (!cfgSpRefresh.length()) {
      h += "<a class='btnlink' href='" + htmlEscape(spAuthUrl()) + "'>Connect Spotify</a>"
           "<small>Press Agree on Spotify's page and you'll be brought straight back here.</small>"
           "<details><summary>The relay page showed a code instead?</summary>"
           "<label>Paste the code or that page's full address</label><input name='url' placeholder='code or full address' autocapitalize='off'>"
           "<button type='submit' class='b2' formaction='/spotify/code' formnovalidate>Use this code</button></details>";
    } else {
      h += "<button type='submit' class='b2' formaction='/spotify/disconnect' formnovalidate>Disconnect Spotify</button>";
    }
  } else if (!portalMode) {
    h += "<small>Save the app details first; a <b>Connect Spotify</b> button then appears here.</small>";
  }

  // ── Motion ──
  h += sec("motion", "Motion");
  if (!imuOk && !portalMode) h += "<small style='color:#e62117'>Motion sensor not detected.</small>";
  h += checkbox("shake", cfgShake, "Shake to refresh");
  h += checkbox("facedown", cfgFaceDown, "Face-down turns the screen off");
  h += checkbox("portrait", cfgPortrait, "Stand it on its side for a tall leaderboard");
  h += checkbox("flip", cfgFlipPortrait, "Tall leaderboard is upside down? Tick to flip it");
  h += checkbox("tap", cfgTap, "Double-tap the desk for the next channel");
  h += "<label>Desk tap sensitivity</label><select name='tapsens'>";
  const char *sens[] = { "", "Low (firm knocks)", "Medium", "High (light taps)" };
  for (int k = 1; k <= 3; k++) h += "<option value='" + String(k) + "'" + (cfgTapSens == k ? " selected" : "") + ">" + sens[k] + "</option>";
  h += "</select>";
  if (!portalMode && imuOk) {
    h += "<label>Calibration</label><small>Put the board in the position you normally use it (on its stand or flat), then press:</small>"
         "<button type='submit' class='b2' formaction='/calibrate' formnovalidate>Set this as the normal position</button>";
  }

  // ── Wi-Fi: saved networks + add/edit one ──
  int edit = server.hasArg("edit") ? server.arg("edit").toInt() : -1;
  if (edit >= numNets) edit = -1;
  Net blank; Net &e = edit >= 0 ? nets[edit] : blank;
  h += sec("wifi", "Wi-Fi networks");
  if (numNets) {
    h += "<small>The board joins your <b>Preferred</b> network when it's in range, otherwise the strongest saved one. "
         "<b>Connect now</b> switches straight away.</small><div class='st' style='margin:10px 0'>";
    for (int k = 0; k < numNets; k++) {
      h += "<div class='saved'><b>" + htmlEscape(nets[k].ssid) + "</b>";
      if (k == curNet && !portalMode) h += "<span style='color:#30d158'>connected</span>";
      else if (!portalMode) h += "<button type='submit' formaction='/wifi/connect?n=" + String(k) + "' formnovalidate class='mini'>Connect now</button>";
      if (nets[k].ip.length()) h += "<span>fixed IP</span>";
      h += "<label style='margin:0'><input type='radio' name='pref' value='" + String(k) + "' style='width:auto'" +
           String(cfgPreferred == k ? " checked" : "") + "> Preferred</label>";
      h += "<a href='/settings?edit=" + String(k) + "#wifi'>Edit</a>";
      h += "<label style='margin:0'><input type='checkbox' name='rm" + String(k) + "' value='1' style='width:auto'> Remove</label></div>";
    }
    h += "<label style='margin:4px 0 0'><input type='radio' name='pref' value='-1' style='width:auto'" +
         String(cfgPreferred < 0 ? " checked" : "") + "> No preference (strongest signal wins)</label>";
    h += "</div>";
  }
  h += "<label>" + String(edit >= 0 ? "Editing: " + htmlEscape(e.ssid) : (numNets ? String("Add another network (e.g. home)") : String("Wi-Fi network"))) + "</label>";
  h += "<button type='button' id='scanBtn' onclick='scan()' class='b2' style='margin-top:6px'>&#128246; Scan for networks</button>";
  h += "<div id='nets' style='margin:8px 0'></div>";
  h += "<input id='s' name='ssid' value='" + htmlEscape(e.ssid) + "' placeholder='Network name (tap one above, or type it)'" + String(numNets ? "" : " required") + ">";
  if (numNets && edit < 0) h += "<small>Leave blank if you're not adding a network.</small>";
  h += "<label>Wi-Fi password</label>";
  h += "<input id='pw' name='pass' type='password' autocomplete='off' autocapitalize='off' placeholder='";
  h += e.pass.length() ? "(saved — leave blank to keep)" : "Password";
  h += "'><small><label style='display:inline;margin:0'><input type='checkbox' style='width:auto' "
       "onclick=\"document.getElementById('pw').type=this.checked?'text':'password'\"> Show password</label></small>";
  h += "<label>Username (only for work Wi-Fi that asks for one)</label>";
  h += "<input name='user' value='" + htmlEscape(e.user) + "' autocapitalize='off' placeholder='Leave blank for normal Wi-Fi'>";
  h += "<details" + String(e.ip.length() || e.compat ? " open" : "") + "><summary>Advanced settings for this network</summary>";
  h += checkbox("compat", e.compat, "Compatibility mode (Wi-Fi 4 instead of Wi-Fi 6)");
  h += "<label>Fixed IP address (blank = automatic)</label>";
  h += "<input name='ip' value='" + htmlEscape(e.ip) + "' placeholder='e.g. 192.168.1.250' inputmode='decimal'>";
  h += "<label>Gateway (router) address</label>";
  h += "<input name='gw' value='" + htmlEscape(e.gw) + "' placeholder='e.g. 192.168.1.1' inputmode='decimal'>";
  h += "<label>Subnet mask</label>";
  h += "<input name='mask' value='" + htmlEscape(e.mask) + "' placeholder='255.255.255.0' inputmode='decimal'>";
  h += "<label>DNS server</label>";
  h += "<input name='dns' value='" + htmlEscape(e.dns) + "' placeholder='e.g. 8.8.8.8' inputmode='decimal'>";
  h += "<small>If this one doesn't answer, the board also tries 8.8.8.8.</small>";
  h += "</details>";

  // ── Security ──
  h += sec("updates", "Updates");
  h += checkbox("autoupd", cfgAutoUpd, "Install new versions from GitHub automatically (at 3 am)");
  h += "<small>Either way the board checks once a day and shows when a new version is out; "
       "install it from the <a href='/update'>Update page</a>.</small>";
  h += "<details><summary>Update source (advanced)</summary><label>Address of the builds folder</label>"
       "<input name='updurl' autocapitalize='off' value='" + htmlEscape(cfgUpdUrl) + "'>"
       "<small>Only change this if you build your own copy (a fork) on GitHub.</small></details>";

  h += sec("security", "Security");
  if (cfgPin.length()) h += "<p class='ok'>PIN is on &#10003;</p>";
  else h += "<small>No PIN set: anyone on your Wi-Fi can open this page and change things.</small>";
  h += "<label>" + String(cfgPin.length() ? "New PIN" : "Choose a PIN (4–12 characters)") + "</label>"
       "<input name='pin' type='password' inputmode='numeric' autocomplete='new-password' maxlength='12' placeholder='" +
       String(cfgPin.length() ? "Leave blank to keep the current PIN" : "e.g. 4 digits") + "'>";
  h += "<label>Type it again</label><input name='pin2' type='password' inputmode='numeric' autocomplete='new-password' maxlength='12'>";
  if (cfgPin.length()) h += checkbox("nopin", false, "Remove the PIN");
  h += "<small>After you save, your browser asks for it whenever you open Settings or Update: "
       "user name <b>admin</b>, password = your PIN. The dashboard stays open to view. "
       "Forgotten it? Hold BOOT for 3 seconds: setup mode doesn't ask for it, so you can change it there.</small>";

  // ── Save ──
  h += "<div class='savebar'><button type='submit'>Save &amp; restart</button></div>";
  h += "</form>";

  // ── Firmware (links only) ──
  if (!portalMode) {
    h += sec("firmware", "Firmware");
    h += "<p style='margin:0'>Running <b>v" FW_VERSION "</b> &middot; <a href='/update'>&#11014; Update</a>" +
         String(updAvail ? " &middot; <b style='color:#30d158'>v" + htmlEscape(updVer) + " available</b>" : "") + "</p>";
    h += sec("backup", "Backup &amp; restore");
    h += "<small>Save all your settings to a file, e.g. before a full re-flash or to set up a second board. "
         "History and records stay on the board.</small>"
         "<a class='btnlink' style='background:#2a2a2e' href='/backup'>&#11015; Download settings</a>"
         "<small><a href='/backup?secrets=1'>Download including passwords, keys and sign-ins</a> – keep that file private.</small>"
         "<label>Restore from a file</label><input type='file' id='rf' accept='.json'>"
         "<button type='button' class='b2' onclick='restore()'>Restore &amp; restart</button><small id='rm'></small>"
         "<script>function restore(){const f=document.getElementById('rf').files[0],m=document.getElementById('rm');"
         "if(!f){m.textContent='Choose the .json file first.';return}f.text().then(t=>fetch('/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:t}))"
         ".then(r=>r.text()).then(t=>{m.textContent=t;setTimeout(()=>location.href='/',12000)}).catch(()=>m.textContent='Restore failed.')}</script>";
  }
  h += "<small style='margin-top:16px'>Board Wi-Fi MAC address: " + boardMac() + "</small></div>";
  h += FPSTR(SCAN_JS);
  if (portalMode) h += "<script>scan()</script>";
  h += "</body></html>";
  server.send(200, "text/html", h);
}

void sendMessage(int code, const String &title, const String &body) {
  server.send(code, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>" + title + "</h1><p>" + body +
              "</p><a href='/settings'>Go back</a></div></body></html>");
}

void handleCalibrate() {
  if (!authed()) return;
  calibrateMotion();
  sendMessage(200, "Calibrated &#10003;", "This is now the board's normal position.");
}

void handleSave() {
  if (!authed()) return;
  { String pin = server.arg("pin"), pin2 = server.arg("pin2"); pin.trim(); pin2.trim();
    if (pin.length() && server.arg("nopin") != "1") {
      if (pin != pin2) { sendMessage(400, "PINs don't match", "Type the same PIN in both boxes. Nothing was saved."); return; }
      if (pin.length() < 4) { sendMessage(400, "PIN too short", "Use at least 4 characters. Nothing was saved."); return; }
    } }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String user = server.arg("user");     user.trim();
  String chs  = server.arg("channels"); chs.trim();
  String key  = server.arg("apikey");   key.trim();
  while (pass.length() && (pass.endsWith("\n") || pass.endsWith("\r") || pass.endsWith(" ") || pass.endsWith("\t")))
    pass.remove(pass.length() - 1);
  while (pass.length() && (pass[0] == ' ' || pass[0] == '\n' || pass[0] == '\r' || pass[0] == '\t'))
    pass.remove(0, 1);
  String twList;
  { String yt, tw; splitLists(chs, yt, tw); yt.trim(); chs = yt;        // a Twitch link pasted in the YouTube box moves across
    twList = normTwitchList(server.arg("twch") + "\n" + tw); }
  bool ytListed = chs.length() > 0;
  if ((!ytListed && !twList.length()) || (ytListed && key.length() == 0 && cfgApiKey.length() == 0)) {
    sendMessage(400, "Missing details", ytListed ? "YouTube channels need the YouTube Data API key." : "Add at least one YouTube or Twitch channel.");
    return;
  }
  String ip = server.arg("ip"), gw = server.arg("gw"), mask = server.arg("mask"), dnsS = server.arg("dns");
  ip.trim(); gw.trim(); mask.trim(); dnsS.trim();
  IPAddress t;
  if ((ip.length() && (!t.fromString(ip) || !t.fromString(gw) || (mask.length() && !t.fromString(mask)))) ||
      (dnsS.length() && !t.fromString(dnsS))) {
    sendMessage(400, "Check the network settings", "A fixed IP needs a valid IP and gateway, e.g. 192.168.1.250 and 192.168.1.1.");
    return;
  }
  // Wi-Fi: remove ticked networks, then add / update the one in the form
  Net keep[MAX_NETS]; int nk = 0;
  int prefOld = server.hasArg("pref") ? server.arg("pref").toInt() : cfgPreferred;
  int prefNew = -1;
  for (int k = 0; k < numNets; k++) if (server.arg("rm" + String(k)) != "1") { if (k == prefOld) prefNew = nk; keep[nk++] = nets[k]; }
  if (ssid.length()) {
    int found = -1;
    for (int k = 0; k < nk; k++) if (keep[k].ssid == ssid) found = k;
    if (found < 0) {
      if (nk >= MAX_NETS) { sendMessage(400, "Too many networks", "You can save up to 5. Remove one first."); return; }
      found = nk++;
      keep[found] = Net();
      keep[found].ssid = ssid;
    }
    Net &n = keep[found];
    if (pass.length()) n.pass = pass;
    n.user = user;
    n.ip = ip; n.gw = ip.length() ? gw : ""; n.mask = ip.length() ? mask : ""; n.dns = dnsS;
    n.compat = server.arg("compat") == "1";
  }
  if (nk == 0) { sendMessage(400, "No Wi-Fi network", "Add at least one Wi-Fi network."); return; }
  for (int k = 0; k < nk; k++) nets[k] = keep[k];
  numNets = nk;
  cfgPreferred = prefNew;
  cfgChannels = chs;
  cfgTwitch = twList;
  if (key.length()) cfgApiKey = key;
  cfgAuto = server.arg("auto") == "1";
  cfgEst = server.arg("est") == "1";
  cfgCelebrate = server.arg("celebrate") == "1";
  cfgLiveAlert = server.arg("livealert") == "1";
  cfgAutoUpd = server.arg("autoupd") == "1";
  { String t = server.arg("ntfy"); t.trim(); t.replace(" ", "-"); cfgNtfy = t;
    String sv = server.arg("ntfysrv"); sv.trim(); if (sv.endsWith("/")) sv.remove(sv.length() - 1);
    if (sv.startsWith("http")) cfgNtfyServer = sv;
    cfgNtfyMask = 0; for (int k = 0; k < 6; k++) if (server.arg("nt" + String(k)) == "1") cfgNtfyMask |= 1 << k; }
  if (server.hasArg("theme")) cfgTheme = server.arg("theme").toInt();
  if (server.hasArg("bright")) cfgBright = server.arg("bright").toInt();
  if (server.hasArg("nbright")) cfgNightBright = server.arg("nbright").toInt();
  applyTheme();
  { String u = server.arg("updurl"); u.trim(); if (u.startsWith("https://")) { if (!u.endsWith("/")) u += "/"; cfgUpdUrl = u; } }
  { String v = server.arg("twid"); v.trim(); if (server.hasArg("twid")) cfgTwId = v;
    v = server.arg("twsec"); v.trim(); if (v.length()) { cfgTwSecret = v; } }
  { String pin = server.arg("pin"), pin2 = server.arg("pin2"); pin.trim(); pin2.trim();
    if (server.arg("nopin") == "1") cfgPin = "";
    else if (pin.length()) cfgPin = pin; }
  cfgSummary = server.arg("summary") == "1";
  cfgShake = server.arg("shake") == "1";
  cfgFaceDown = server.arg("facedown") == "1";
  cfgPortrait = server.arg("portrait") == "1";
  cfgFlipPortrait = server.arg("flip") == "1";
  cfgTap = server.arg("tap") == "1";
  if (server.hasArg("idleclk")) cfgIdleClock = constrain(server.arg("idleclk").toInt(), 0, 60);
  String town = server.arg("wxtown"); town.trim();
  if (town.length() && town != cfgWxName && !portalMode) {
    float la, lo; String label;
    if (geocode(town, la, lo, label)) { cfgWxLat = la; cfgWxLon = lo; cfgWxName = label; }
    else { sendMessage(400, "Town not found", "Couldn't find \"" + htmlEscape(town) + "\". Try a nearby town or add the country, e.g. \"Paisley, UK\"."); return; }
  } else if (town.length() && portalMode && town != cfgWxName) { cfgWxName = town; cfgWxLat = cfgWxLon = 0; }   // looked up once online
  String spid = server.arg("spid"); spid.trim();
  String spsec = server.arg("spsecret"); spsec.trim();
  if (spid != cfgSpId) { cfgSpId = spid; cfgSpRefresh = ""; }
  if (spsec.length()) cfgSpSecret = spsec;
  String spredir = server.arg("spredir"); spredir.trim();
  if (spredir != cfgSpRedirect) { cfgSpRedirect = spredir; cfgSpRefresh = ""; }
  if (server.hasArg("tapsens")) cfgTapSens = constrain(server.arg("tapsens").toInt(), 1, 3);
  if (server.hasArg("raceA")) { cfgRaceA = server.arg("raceA").toInt(); cfgRaceB = server.arg("raceB").toInt(); }
  if (server.hasArg("nightStart")) { cfgNightStart = server.arg("nightStart").toInt(); cfgNightEnd = server.arg("nightEnd").toInt(); }
  saveSettings();
  sendMessage(200, "Saved &#10003;", "The board is restarting and will join whichever saved network is in range.");
  drawStatus("Saved!", "Restarting...", C_GREEN);
  delay(1500);
  ESP.restart();
}

// "Connect now": answer the browser first (we're about to leave this network), then switch in loop()
void handleWifiConnect() {
  if (!authed()) return;
  int k = server.arg("n").toInt();
  if (k < 0 || k >= numNets) { sendMessage(400, "Unknown network", "That network isn't saved."); return; }
  sendMessage(200, "Switching to " + htmlEscape(nets[k].ssid) + "&hellip;",
              "The board is leaving this network now, so this page will stop updating. "
              "Join <b>" + htmlEscape(nets[k].ssid) + "</b> yourself and use the new address shown on the board's leaderboard. "
              "If it can't connect, it goes back to the best network it can find.");
  pendingSwitch = k;
}

void handleSpotifyCode() {
  if (!authed()) return;
  String err;
  if (spConnectWithCode(server.arg("url"), err)) {
    saveSettings();
    sendMessage(200, "Spotify connected &#10003;", "Long-press the board's screen and choose Spotify.");
    if (app == APP_SP && !menuOpen) { lastSpPoll = 0; }
  } else sendMessage(400, "Couldn't connect Spotify", htmlEscape(err) + ". Codes only work once and expire after a few minutes &ndash; open the Spotify link again and paste the new address.");
}

// The relay page sends the browser here with ?code=... after you press Agree on Spotify
void handleSpotifyCallback() {
  if (!authed()) return;
  if (server.hasArg("error")) { sendMessage(400, "Spotify not connected", "Spotify said: " + htmlEscape(server.arg("error"))); return; }
  String err;
  if (spConnectWithCode(server.arg("code"), err)) {
    saveSettings();
    if (app == APP_SP && !menuOpen) lastSpPoll = 0;
    server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>Spotify connected &#10003;</h1><p>Long-press the board's screen and choose Spotify.</p>"
                "<a href='/'>Go to the dashboard</a></div></body></html>");
  } else sendMessage(400, "Couldn't connect Spotify", htmlEscape(err) + ". Press Connect Spotify on the settings page to try again.");
}

void handleSpotifyDisconnect() {
  if (!authed()) return;
  cfgSpRefresh = ""; sp = SpotifyState();
  saveSettings();
  sendMessage(200, "Spotify disconnected", "You can connect it again any time.");
}

void handleNotFound() {
  if (!portalMode) { server.send(404, "text/plain", "Not found"); return; }
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

void registerRoutes() {
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.on("/api/csv", HTTP_GET, handleApiCsv);
  server.on("/backup", HTTP_GET, handleBackup);
  server.on("/ntfy/test", HTTP_POST, handleNtfyTest);
  server.on("/restore", HTTP_POST, handleRestore);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/update/github", HTTP_POST, handleUpdateGithub);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/spotify/code", HTTP_POST, handleSpotifyCode);
  server.on("/spotify/callback", HTTP_GET, handleSpotifyCallback);
  server.on("/spotify/disconnect", HTTP_POST, handleSpotifyDisconnect);
  server.on("/favicon.svg", HTTP_GET, handleIconSvg);
  server.on("/favicon.png", HTTP_GET, handleIconPng);
  server.on("/favicon.ico", HTTP_GET, handleIconPng);          // older browsers ask for this; PNG inside is fine
  server.on("/apple-touch-icon.png", HTTP_GET, handleTouchIcon);
  server.on("/apple-touch-icon-precomposed.png", HTTP_GET, handleTouchIcon);
  server.onNotFound(handleNotFound);
}
