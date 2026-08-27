# UI-AUDIT2D — Mobbin invocation log (hard gate evidence)

This file records the **actual** plugin identity, the **actual** queries issued and
the **actual** references returned. Nothing here is recalled from memory. No web
search was used anywhere in this audit, and no non-Mobbin source is cited as
Mobbin.

## 1. Plugin / tool identity, as exposed to Claude Code

The Mobbin MCP server was connected in this session (`/mcp` reported
"Authentication successful. Connected to mobbin."). Its tools were surfaced to the
agent as **deferred** tools — name only, no schema — and had to be loaded before
any call could be made:

```
ToolSearch(query="select:mcp__mobbin__search_screens,mcp__mobbin__search_flows,mcp__mobbin__search_sections",
           max_results=5)
```

Exact exposed tool identities returned by that call:

| exposed tool name | kind | required params | notable params |
| --- | --- | --- | --- |
| `mcp__mobbin__search_screens` | single-screen search | `query`, `platform` | `platform` in {`ios`,`web`}; `limit` 1-30 (default 20); `mode` in {`deep`,`standard`,`fast`} default `deep`; `exclude_screen_ids`; `image_format` in {`webp`,`jpg`} |
| `mcp__mobbin__search_flows` | multi-step flow search | `query`, `platform` | `limit` 1-10 (default 5); `page` 1-20 |
| `mcp__mobbin__search_sections` | website-section search | `query` | `limit` 1-30; `page` |

Server namespace prefix: `mcp__mobbin__`. The platform enum is **`ios` or `web`
only** — there is no `android` and no `ipad`/`tablet` value. This is a real and
material limitation for this audit and is restated in the report: every mobile
reference below is an **iOS phone** capture. `search_sections` was deliberately
**not** invoked — it searches marketing website sections (Pricing, Footer, Hero),
which has no bearing on a native 3D editor's chrome.

Each result carries a per-screen `id` (UUID), an `image_url`, a canonical
`mobbin_url`, `app_name` and `platform`. The images were returned inline and were
**examined**; every classification in the report is written from the pixels, not
from the metadata.

## 2. Queries issued, in order

All screen searches ran in the default `deep` mode. Every query targets exactly
one screen concept, per the tool's own guidance.

| # | tool | `platform` | `limit` | query string (verbatim) |
| --- | --- | --- | --- | --- |
| Q01 | `search_screens` | ios | 6 | `floating contextual toolbar over a canvas with tool icons, in a drawing or design app` |
| Q02 | `search_screens` | ios | 6 | `tablet side inspector panel with layer properties next to a canvas` |
| Q03 | `search_screens` | ios | 6 | `bottom sheet with editable numeric input fields and unit values for object dimensions` |
| Q04 | `search_screens` | ios | 6 | `layers list panel showing stacked objects with visibility toggles in an editor` |
| Q05 | `search_screens` | ios | 6 | `3D model viewer or AR object placement with rotation and scale controls` |
| Q06 | `search_screens` | ios | 6 | `photo editor with a brush size slider and strength slider along the edge of the image` |
| Q07 | `search_screens` | ios | 6 | `version history panel with undo redo and a list of recent edits` |
| Q08 | `search_screens` | ios | 6 | `project file browser home screen with a grid of design document thumbnails and a new project button` |
| Q09 | `search_screens` | ios | 6 | `settings and preferences screen with grouped rows and toggles in a creative app` |
| Q10 | `search_screens` | ios | 6 | `segmented control switching between edit modes at the top of a full screen editor` |
| Q11 | `search_screens` | ios | 6 | `long press context menu with grouped destructive and duplicate actions over selected content` |
| Q12 | `search_flows` | ios | 3 | `creating a new project and choosing a starting template in a design tool` |
| Q13 | `search_screens` | ios | 5 | `video editing timeline with dense tool controls and precise trim handles` |
| Q14 | `search_screens` | ios | 5 | `measuring tool showing dimension labels overlaid on a real world object` |

**14 invocations here**, plus one re-verification call (Q15, recorded under Q05
below) for **15 total** — 14 screen searches and 1 flow search, covering all ten pattern
families the brief required, plus two extra families — dense timeline tooling and
on-scene measurement — chosen because they are the closest analogues in the
library to a CAD-adjacent viewport tool.

## 3. Returned references, by query

Every row below was returned by the query named. The `inspected` column says
whether the returned image was actually examined and reasoned about; a returned
result that was looked at and judged irrelevant is recorded as `seen, not used`
with its reason rather than silently dropped.

### Q01 — floating contextual toolbar

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Apple Mail | `2083e4cb-8da4-4bb3-879a-ad3e074ccf60` | https://mobbin.com/screens/2083e4cb-8da4-4bb3-879a-ad3e074ccf60 | **yes — R01** |
| Craft | `6011086c-42ca-43e5-aa73-2d1d6de04934` | https://mobbin.com/screens/6011086c-42ca-43e5-aa73-2d1d6de04934 | **yes — R02** |
| Freeform | `c4e0c434-c708-470e-9ad2-810bff80975b` | https://mobbin.com/screens/c4e0c434-c708-470e-9ad2-810bff80975b | **yes — R03** |
| Obsidian | `f660ba6a-02b7-4f05-9103-2b13a84763dd` | https://mobbin.com/screens/f660ba6a-02b7-4f05-9103-2b13a84763dd | **yes — R04** |
| Apple Notes | `83057132-c8b4-4079-a4c9-9cf8baffad03` | https://mobbin.com/screens/83057132-c8b4-4079-a4c9-9cf8baffad03 | **yes — R05** |
| Spotify | `6d308882-4af3-46ec-a0e3-de12afa09c6a` | https://mobbin.com/screens/6d308882-4af3-46ec-a0e3-de12afa09c6a | **yes — R06** |

### Q02 — tablet side inspector

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Play | `c6722fdf-fce5-4cda-bf71-f6385a4cbe35` | https://mobbin.com/screens/c6722fdf-fce5-4cda-bf71-f6385a4cbe35 | **yes — R07** |
| Freeform | `5d96bab5-b90b-4f50-9caa-1b1f98477224` | https://mobbin.com/screens/5d96bab5-b90b-4f50-9caa-1b1f98477224 | **yes — R08** |
| Photoroom | `aaad7e92-d1cb-49b0-a9d0-d3966e03d5b7` | https://mobbin.com/screens/aaad7e92-d1cb-49b0-a9d0-d3966e03d5b7 | **yes — R09** |
| Pinterest | `8544cee9-03f9-4375-a777-15253d3ff7fd` | https://mobbin.com/screens/8544cee9-03f9-4375-a777-15253d3ff7fd | **yes — R10** |
| Spotify | `feaa21a0-c822-4f1f-b722-3bd2ed256f41` | https://mobbin.com/screens/feaa21a0-c822-4f1f-b722-3bd2ed256f41 | **yes — R11** |
| Grok Bot | `62a9240b-ff9f-4694-9c83-5589d6a24d3d` | https://mobbin.com/screens/62a9240b-ff9f-4694-9c83-5589d6a24d3d | seen, not used — a letterboxed desktop screen recording, not a tablet inspector |

### Q03 — numeric editing sheet

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Play | `4449eae8-a1fe-4fcd-8f01-27e5b90a0e1d` | https://mobbin.com/screens/4449eae8-a1fe-4fcd-8f01-27e5b90a0e1d | **yes — R12**, the single highest-value reference in this audit |
| Crouton | `80e0c9db-24c5-4023-83e0-00c9750749d4` | https://mobbin.com/screens/80e0c9db-24c5-4023-83e0-00c9750749d4 | **yes — R13** |
| Alma | `69dc77f5-5494-410a-be7e-7325839d24f8` | https://mobbin.com/screens/69dc77f5-5494-410a-be7e-7325839d24f8 | **yes — R14** |
| Tinder | `ef5d6545-2509-48da-8cca-184c30275d2a` | https://mobbin.com/screens/ef5d6545-2509-48da-8cca-184c30275d2a | **yes — R15** |
| Alta | `0cb86101-485a-45c9-8ee8-8c136355b15b` | https://mobbin.com/screens/0cb86101-485a-45c9-8ee8-8c136355b15b | **yes — R16** |
| Yazio | `b6db4cbe-ece5-49fe-a61a-d37f677028fe` | https://mobbin.com/screens/b6db4cbe-ece5-49fe-a61a-d37f677028fe | seen, not used — quantity + unit picker, subsumed by R13/R15 |

### Q04 — object / layer list

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Play | `c27eecd0-2d73-4c63-b672-11aab4d0195e` | https://mobbin.com/screens/c27eecd0-2d73-4c63-b672-11aab4d0195e | **yes — R17** |
| Photoroom | `17875c39-0946-4b6b-a760-bb3868918e01` | https://mobbin.com/screens/17875c39-0946-4b6b-a760-bb3868918e01 | **yes — R18** |
| Notion | `cc8b241a-76d5-4192-8a33-5b970ea2f9ab` | https://mobbin.com/screens/cc8b241a-76d5-4192-8a33-5b970ea2f9ab | **yes — R19** |
| Bevel | `dccd25d2-dba3-4fb7-8c36-4b31d616e94d` | https://mobbin.com/screens/dccd25d2-dba3-4fb7-8c36-4b31d616e94d | **yes — R20** |
| Timepage | `7fca0f42-0b5b-4574-807f-c0758a1cfa98` | https://mobbin.com/screens/7fca0f42-0b5b-4574-807f-c0758a1cfa98 | seen, not used — calendar visibility, same principle as R19 |
| Grab | `c8562675-9d02-4d85-b54e-5776d6bea80f` | https://mobbin.com/screens/c8562675-9d02-4d85-b54e-5776d6bea80f | seen, not used — map layer toggles, subsumed by R19 |

### Q05 — 3D / AR object manipulation

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Best Buy | id begins `e63249bf` and ends `983eff1d6d70`; image short-link `EDaV0Q2o` | see the Q15 re-verification below | **yes — R21** |
| Target | `e533bff5-81a1-4bb2-bff7-75795b655d35` | https://mobbin.com/screens/e533bff5-81a1-4bb2-bff7-75795b655d35 | **yes — R22**, catalogued as a rejected pattern |
| Apple Store | `4f6a44a6-123f-4deb-a7cf-6831f34a9130` | https://mobbin.com/screens/4f6a44a6-123f-4deb-a7cf-6831f34a9130 | **yes — R23** |
| Crate & Barrel | `86b6fa71-84cc-4438-bfc6-222be6c30a6c` | https://mobbin.com/screens/86b6fa71-84cc-4438-bfc6-222be6c30a6c | **yes — R24** |
| Tonal | `5aa43142-889e-44d8-9a74-33bc0e55779a` | https://mobbin.com/screens/5aa43142-889e-44d8-9a74-33bc0e55779a | **yes — R25** |
| Replika | `904b389b-f195-4e47-a851-666b71fba83f` | https://mobbin.com/screens/904b389b-f195-4e47-a851-666b71fba83f | seen, not used — 3D avatar with no manipulation chrome |

**Note on the Best Buy id, and one deliberate re-verification (Q15).** The same
Best Buy AR screen was returned under both Q05 (`image_url` short-link
`EDaV0Q2o`) and Q14 (short-link `Ys8cMeX7`). Rather than re-type a 36-character
UUID from a scrolled result and risk publishing a wrong identifier, a fifteenth
invocation was made to obtain a Best Buy AR reference whose id is verified in
full:

| # | tool | `platform` | `limit` | `mode` | query string (verbatim) |
| --- | --- | --- | --- | --- | --- |
| Q15 | `search_screens` | ios | 1 | `standard` | `Best Buy AR product placement with dimension labels and a selection bounding box` |

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Best Buy | `f1bd0ab1-88d5-42a4-9782-7aa3ae1da7bb` | https://mobbin.com/screens/f1bd0ab1-88d5-42a4-9782-7aa3ae1da7bb | **yes — R21b** |

R21b is the *same product and the same interaction*, captured with the dimension
overlay **off**: the bounding box and the four-icon contextual toolbar attached to
the box are present, the in-scene dimension pills are not. That difference is
itself useful and is used in the report — it shows that the reference treats
on-scene dimensions as a *toggleable overlay*, not as permanent chrome. Where the
report cites a Best Buy dimension pill it means R21 (dimensions on); where it
cites the bounding box and attached toolbar it means R21b, whose id is verified.

### Q06 — brush / strength sliders

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Edits | `36103f79-daaa-4363-82e0-9832c0b4003f` | https://mobbin.com/screens/36103f79-daaa-4363-82e0-9832c0b4003f | **yes — R26** |
| Google Arts & Culture | `46d62d2c-6ac2-47ae-958a-f7cdcc07cb19` | https://mobbin.com/screens/46d62d2c-6ac2-47ae-958a-f7cdcc07cb19 | **yes — R27** |
| Telegram | `dc3a6820-6750-4363-9f63-be2ad3b6e17b` | https://mobbin.com/screens/dc3a6820-6750-4363-9f63-be2ad3b6e17b | **yes — R28** |
| Apple Photos | `0dc105f4-8259-4396-bb88-979b3c4d9a66` | https://mobbin.com/screens/0dc105f4-8259-4396-bb88-979b3c4d9a66 | **yes — R29** |
| eBay | `6652fd22-df74-45c3-a829-8783b28db867` | https://mobbin.com/screens/6652fd22-df74-45c3-a829-8783b28db867 | **yes — R31** |
| Shopee | `e0a34c4b-29b8-4bdf-aced-9fb455d9aa09` | https://mobbin.com/screens/e0a34c4b-29b8-4bdf-aced-9fb455d9aa09 | **yes — R30** |

### Q07 — history / version controls

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Wabi | `f9776c91-0b37-4d38-af76-a74b90fddbef` | https://mobbin.com/screens/f9776c91-0b37-4d38-af76-a74b90fddbef | **yes — R32** |
| Canva | `cedddc9d-109c-45cf-9e9e-f7905a0653f0` | https://mobbin.com/screens/cedddc9d-109c-45cf-9e9e-f7905a0653f0 | **yes — R33** |
| Mimo | `cbc37c50-daee-45b7-b2b9-f37fd14cb759` | https://mobbin.com/screens/cbc37c50-daee-45b7-b2b9-f37fd14cb759 | **yes — R34** |
| Obsidian | `a41cd3b3-79a3-4fbb-833c-c2a42655fec7` | https://mobbin.com/screens/a41cd3b3-79a3-4fbb-833c-c2a42655fec7 | **yes — R35** |
| Squarespace | `6d2f83d1-3ad8-41d5-aaaa-e79478063ed2` | https://mobbin.com/screens/6d2f83d1-3ad8-41d5-aaaa-e79478063ed2 | **yes — R36** |
| Replika | `ec83c71a-089b-42b5-affc-7e7b9ecc1b14` | https://mobbin.com/screens/ec83c71a-089b-42b5-affc-7e7b9ecc1b14 | seen, not used — a build-channel picker labelled "Version history"; not an edit history |

### Q08 — project / home hub

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Edits | `549d50bb-ddf3-44e0-8ffd-704db6bf2afe` | https://mobbin.com/screens/549d50bb-ddf3-44e0-8ffd-704db6bf2afe | **yes — R37** |
| Riveo | `8ceb8a17-6494-4a42-8bdd-efe289efc913` | https://mobbin.com/screens/8ceb8a17-6494-4a42-8bdd-efe289efc913 | **yes — R38** |
| ElevenLabs | `0d2a3abe-9843-47cb-995d-b3a89c611820` | https://mobbin.com/screens/0d2a3abe-9843-47cb-995d-b3a89c611820 | **yes — R39** |
| CapCut | `0faea8ac-1eb1-40d2-bb07-76e7ae9f8809` | https://mobbin.com/screens/0faea8ac-1eb1-40d2-bb07-76e7ae9f8809 | **yes — R40** |
| Canva | `55975230-ad04-4689-83e9-81363662bc5d` | https://mobbin.com/screens/55975230-ad04-4689-83e9-81363662bc5d | **yes — R41** |
| Google Drive | `62f3913a-4115-4109-a77a-61e27c6255f3` | https://mobbin.com/screens/62f3913a-4115-4109-a77a-61e27c6255f3 | **yes — R42** |

### Q09 — settings / preferences

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| GitHub | `81f621f2-2c0c-4877-a302-d58c189bcd61` | https://mobbin.com/screens/81f621f2-2c0c-4877-a302-d58c189bcd61 | **yes — R43** |
| Obsidian | `54c00af4-c143-4537-86a0-30acf08e709f` | https://mobbin.com/screens/54c00af4-c143-4537-86a0-30acf08e709f | **yes — R44** |
| Google TV | `13577f13-8ff2-4789-ab1b-5a95a0df46d8` | https://mobbin.com/screens/13577f13-8ff2-4789-ab1b-5a95a0df46d8 | **yes — R45** |
| MasterClass | `7e75066f-5638-4dee-847f-7b83ba5a7be9` | https://mobbin.com/screens/7e75066f-5638-4dee-847f-7b83ba5a7be9 | **yes — R46** |
| Wispr Flow | `af36a034-bc88-48f0-90eb-8af699250f0d` | https://mobbin.com/screens/af36a034-bc88-48f0-90eb-8af699250f0d | **yes — R47** |
| Replika | `cf737477-0f93-4ff4-bbd3-b2c85da76a3f` | https://mobbin.com/screens/cf737477-0f93-4ff4-bbd3-b2c85da76a3f | **yes — R48** |

### Q10 — mode switcher / segmented control

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Craft | `40835953-b578-4b9c-b486-b67f7a61b0a8` | https://mobbin.com/screens/40835953-b578-4b9c-b486-b67f7a61b0a8 | **yes — R49** |
| Google Photos | `a00a0184-7311-40ec-95e8-c167bf17134c` | https://mobbin.com/screens/a00a0184-7311-40ec-95e8-c167bf17134c | **yes — R50** |
| Linktree | `a8ec77da-d66a-4d33-b975-08d4a35054fa` | https://mobbin.com/screens/a8ec77da-d66a-4d33-b975-08d4a35054fa | **yes — R51** |
| Bears Gratitude | `84c8c926-396b-4dba-b117-3682dc94eaf4` | https://mobbin.com/screens/84c8c926-396b-4dba-b117-3682dc94eaf4 | **yes — R52** |
| Posh | `ce0690e7-3a72-462b-9687-5221f68de716` | https://mobbin.com/screens/ce0690e7-3a72-462b-9687-5221f68de716 | **yes — R53** |
| Vocabulary | `5c9c2eac-0d56-4cf4-85c4-62834a6c761a` | https://mobbin.com/screens/5c9c2eac-0d56-4cf4-85c4-62834a6c761a | seen, not used — two-segment theme scope, subsumed by R49 |

### Q11 — contextual action menu

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Freeform | `0dc02572-9108-40fd-addc-f3973a4a70a8` | https://mobbin.com/screens/0dc02572-9108-40fd-addc-f3973a4a70a8 | **yes — R54** |
| Obsidian | `09dec6de-5d1b-4020-922b-ad238e77263c` | https://mobbin.com/screens/09dec6de-5d1b-4020-922b-ad238e77263c | **yes — R55** |
| Craft | `86720adc-8799-4a61-8db6-23dd57d17172` | https://mobbin.com/screens/86720adc-8799-4a61-8db6-23dd57d17172 | **yes — R56** |
| Play | `f53a80a5-6bbe-4b43-a78c-1152bd2e8121` | https://mobbin.com/screens/f53a80a5-6bbe-4b43-a78c-1152bd2e8121 | **yes — R57** |
| Notion | `aeb6d373-0c84-4b69-a591-029ea8938b83` | https://mobbin.com/screens/aeb6d373-0c84-4b69-a591-029ea8938b83 | **yes — R58** |
| yope | `ba6c824d-7f62-4b7e-9c38-daf556016481` | https://mobbin.com/screens/ba6c824d-7f62-4b7e-9c38-daf556016481 | seen, not used — chat reply menu, no editor relevance |

### Q12 — flow: creating a new project

| app | flow id | mobbin_url | screens | inspected |
| --- | --- | --- | --- | --- |
| Play | `e0925e02-a97e-4e84-813d-3444f981c257` | https://mobbin.com/flows/e0925e02-a97e-4e84-813d-3444f981c257 | 4 | **yes — F01** |
| Unfold | `7c11e43c-816a-4ec5-bba0-dbc5e6d45a25` | https://mobbin.com/flows/7c11e43c-816a-4ec5-bba0-dbc5e6d45a25 | 4 | **yes — F02** |
| Play | `f503d6bf-b7bc-4652-8ad0-0648a64e5b9f` | https://mobbin.com/flows/f503d6bf-b7bc-4652-8ad0-0648a64e5b9f | 4 | duplicate of F01 — identical four screen ids; noted, not double-counted |

### Q13 — dense timeline tooling

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| GoPro Quik | `78ba6d70-7b23-4ef6-94bd-46247224fdbf` | https://mobbin.com/screens/78ba6d70-7b23-4ef6-94bd-46247224fdbf | **yes — R59** |
| Netflix | `ab3d66cc-017a-4f8e-9476-779e7f21d7ad` | https://mobbin.com/screens/ab3d66cc-017a-4f8e-9476-779e7f21d7ad | **yes — R60** |
| Shopee | `7a019ddb-3b5e-43b1-8947-f2cdca95b9fc` | https://mobbin.com/screens/7a019ddb-3b5e-43b1-8947-f2cdca95b9fc | **yes — R61** |
| Polarsteps | `b6fb5c67-1b0c-491b-974e-f3520ae4bee9` | https://mobbin.com/screens/b6fb5c67-1b0c-491b-974e-f3520ae4bee9 | seen, not used — trim handles only, subsumed by R60 |
| X | `f1adee04-7fdf-4158-a5a9-7fe2c9190b18` | https://mobbin.com/screens/f1adee04-7fdf-4158-a5a9-7fe2c9190b18 | seen, not used — minimal trimmer, nothing to add |

### Q14 — on-scene measurement

| app | screen id | mobbin_url | inspected |
| --- | --- | --- | --- |
| Redfin | `27208efe-2b6f-4315-9f17-892cb418ac05` | https://mobbin.com/screens/27208efe-2b6f-4315-9f17-892cb418ac05 | **yes — R62** |
| Target | `3cbe4009-621d-4f44-a446-2b26dd017b16` | https://mobbin.com/screens/3cbe4009-621d-4f44-a446-2b26dd017b16 | **yes — R63** |
| Bump | `a933354a-4184-4aa4-a6ec-e52a4e3970c5` | https://mobbin.com/screens/a933354a-4184-4aa4-a6ec-e52a4e3970c5 | **yes — R64** |
| Best Buy | (same screen as R21) | — | already inspected under Q05 |
| Apple Store | `4adb6dcc-b91b-4ef0-b296-092fb82a8b5f` | https://mobbin.com/screens/4adb6dcc-b91b-4ef0-b296-092fb82a8b5f | seen, not used — AR scanning coach state, no manipulation chrome |

## 4. Totals

- **15 Mobbin invocations. 0 web searches. 0 non-Mobbin sources.**
- 81 result rows returned; **65 screen references inspected and catalogued
  (R01-R64 plus R21b) and 2 flows (F01-F02)**; 12 recorded as `seen, not used`
  with the reason; 1 duplicate flow identified and not double-counted.
- Platform of every mobile reference: **iOS phone**. The tool exposes no Android
  and no tablet/iPad platform value, so no reference in this audit is an Android
  capture and none is a true tablet capture. Every phone-versus-tablet and
  touch-versus-stylus claim in the report is therefore *reasoned*, and is labelled
  as reasoned rather than observed.
