# FoloToy AI Passport 上传（发布）规则与注意事项

> 适用：通过官方发布助手向 <https://ai-passport.folotoy.cn> 提交 / 更新社区玩法（固件类）。
> 本文由本次 **374 六爻占卜 / I Ching Divination** 的发布实践整理，含踩坑记录。

## 0. 一句话流程

```
authorize --code-stdin  →  whoami 核对  →  projects 核对
   →  备料（0x0 合并整包固件 + 竖版 3:4 封面 + 中英文案）
   →  validate  →  submit --project-id <id> --auto  →  projects 报真实状态
```

## 1. 授权

- 用官方发布页生成的一次性码，必须走 `authorize --code-stdin`（通过 stdin 私密输入），**不要另开浏览器授权**。
- 所有命令统一使用 `FOLOTOY_AI_PASSPORT_URL=https://ai-passport.folotoy.cn`。
- 凭据保存在**仓库外**（如 `FOLOTOY_PUBLISHER_CONFIG=/tmp/.../config.json`），不写进命令参数、不打印、不提交。
- 一次性码 24 小时过期、只可兑换一次；token 同期限，可在官网吊销。
- `whoami` 必须先核对：`scope` 与 `projectId` 与官方提示一致
  （更新 = `update_once` + `projectId`；新建 = `create_once`）→ **不符立即停止**，不改 `--confirmed`、不新建。

## 2. 目标核对（projects）

- 用 `projects` 按 **ID + slug** 匹配，**不要只凭相似标题**。
- 记录：标题(zh/en)、简介、分类、仓库 URL、最新版本(`revisionId` / `status` / 固件 `sha256`)。
- 若存在 **draft / pending** 版本 → 停止并报告冲突，交作者在详情页处理；**不得覆盖 / 撤回 / 删除 / 新建**。

## 3. 固件（最容易翻车的一步）

- **必须是「从 `0x0` 起烧」的合并整包**（bootloader + 分区表 + app 三段合一），**不是** app 分区镜像。
- 校验：`validate --firmware <整包> --cover <封面>` 应显示 `segments: 3`。
- 大小限制：非空、≤ 8 MiB。
- 本工程用 `fw/dist/FoloToy-AI-Passport-2.0.bin`（由 `fw/build.sh` 的 `idf.py merge-bin` 产出）。
  - ⚠️ **不要传** `fw/build/FoloToy-AI-Passport.bin` —— 那只是 **app 分区**（只能烧到 `0x10000`）。
- **教训**：曾误传 app 分区 bin（1287216 B），审核侧按 `0x0` 烧录无法启动 → `rejected`「无法运行」。

## 4. 封面（必填）

- 竖版 **3:4**，JPEG / PNG / WebP，≤ 10 MiB。
  本工程：`fw/cover/submit/cover.png`（825×1100）。
- 用已有素材或生成；都没有且无法生图时，**向用户要一张**。不可省略、不可造假截图、不可用无关占位图。
- 额外玩法图**可选**（最多 4 张，同样 3:4）；不要求实机截图。

## 5. 分类与文案

- 分类从 `games / productivity / information / learning / media / social / developer` 选；**游戏不要放 developer**。
- 文案「**玩法优先**」：是什么、怎么玩、哪里有趣。
- **禁止**硬件与框架术语：芯片/板子/厂商/编解码/总线/引脚/内存闪存/分区/SDK/框架等
  （如 `ESP32`、`ESP32-C3`、`ES8311`、`I2C`、`I2S`、`SPI`、`GPIO`、`OLED`、`LCD`、`LVGL`）。
- 中英双语；英文是**自然产品文案**，不是逐字翻译。

## 6. 提交

- `validate` 通过后再提交：`submit --project-id <id> --auto`（预授权场景）。
- 完整字段：中英文标题 / 简介 + `--category` + `--cover` + `--firmware`（可选 `--source-url` / `--image`）。
- 当前接口要求**完整资料 + 封面 + 固件**，**不能假定省略会自动沿用**旧图 / 旧文案。

## 7. 不确定响应 / 重试

- 响应不确定：先用**同一 token** 跑 `projects` 看 `submissionResult`。
- 有结果 → 报告原提交，**不重复提交**。
- 无结果 → 仅用**完全相同的内容** + 同一 token 重试；内容变了会被 `409` 拒。
- **绝不**为了重试而新建授权 / 项目；授权过期就请用户重新生成官方提示码。

## 8. 结果报告

- 报告：`projectId`、`revisionId`、`slug`、**真实审核状态**。
- `pending` = 已提交**待审**，**不等于已公开**；`rejected` 要说明原因、解决根因，**不自动重试**。

## 9. 本工程专属核对清单

- [ ] `whoami`：`scope=update_once`、`projectId=374`
- [ ] `projects`：目标 `374`、`slug=community-82c0f6fb`、`category=learning`、仓库 `ai-passport-liuyao`
- [ ] 无 `pending` / `draft` 冲突
- [ ] 固件用 `fw/dist/FoloToy-AI-Passport-2.0.bin`（`segments=3`）
- [ ] 封面 `fw/cover/submit/cover.png`（3:4）
- [ ] 文案无硬件 / 框架术语
- [ ] 改过 UI 文案后跑 `python3 tools/gen_fonts.py` + `python3 tools/check_font_coverage.py`（防真机方块字）
- [ ] 工程目录整洁：**勿**把旧 1.0 固件 / 其它 `firmware.bin` 混进来
- [ ] 提交前经用户确认

## 10. 红线

- 不外传 / 不打印凭据；不批准 moderation；不刷量；不绕过服务端校验。
- 不自动删除 / 撤回 / 新建；不把待审说成已公开。
