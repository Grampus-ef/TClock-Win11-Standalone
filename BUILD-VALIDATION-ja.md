# TClock11 Standalone — ビルド検証記録

**検証日:** 2026-10-15（日本時間）
**対象:** Windows 11向け独立オーバーレイ試作版（Explorer注入なし）

## GitHub Actionsで確認した事実

- Windowsランナー: `windows-2022`
- コンパイラ: MSVC 19.44.35229 (x64), Visual Studio 2022
- Windows SDK: 10.0.26100.0
- ビルド形式: C++17, x64, Windows GUI subsystem
- ビルド済み: `TClock11Standalone.exe` 212,992バイト
- PEヘッダー検査: Machine=AMD64、PE32+、Subsystem=Windows GUI **PASS**
- 最終CI: [実行 38605954504](https://github.com/Grampus-ef/TClock-Win11-Standalone/actions/runs/38605954504) **success**
- 最終コンパイラ警告/エラー: 検証ログ上なし
- Artifact: `TClock11Standalone-Windows-x64-prototype` (ID: 11914989688)
- Artifact SHA256: `45c3db73f3d904d236f02d77e0f02ad0be6500fba2b424567449880bdae8d8b9`（Artifact ZIP全体のSHA256）

## 不具合修正の経緯

| 実行ID | 結果 | 判明した内容と対応 |
| --- | --- | --- |
| 38605311373 | failure | `cl /Bv` はソースなしでD8003。誤った環境検査を除去 |
| 38605477088 | failure | `GetIfTable2`/ `PMIB_IF_TABLE2` 未宣言。winsock/WS2IPDEFヘッダーを先に追加 |
| 38605706345 | success | x64 EXEを生成、PE検査、Artifact公開まで成功 |
| 38605954504 | success | UNICODE重複定義警告を解消。コンパイラ警告・エラーなしで再成功 |

## 未検証（成功とは扱わない）

- Windows 11 24H2/25H2以降での実機起動、描画、クリック処理
- タスクバーとの実際の重なり、配置、全画面アプリ中の動作
- DPI倍率変更、複数モニター、自動非表示タスクバー
- Windows Update後の継続動作、メモリ/CPU/通信速度表示の正確性
- 元TClock-Win10との表示/マウス操作/書式の完全互換性

CIランナーはWindows Server 2022であり、**Windows 11実機動作検証の代用にはならない**。

## 重要な配布上の注意

既存のTClock-Win10 exe/DLLは変更していない。
このGitHubリポジトリへは独立版のビルドに必要なファイルだけを登録。
提供された元ソースおよび修正前ソース全体のZIPは、引継ぎ資料内に別途保持する。
試作版を既存TClockやExplorerPatcherと同時に本番環境で動かす前に、
非本番のWindows 11検証環境で確認すること。
