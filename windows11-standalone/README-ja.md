# TClock11 Standalone：Windows 11独立オーバーレイ試作版

**重要：独立版の試作であり、TClock-Win10完全互換版ではありません。**

Windows 11の標準タスクバー付近に時計を重ねて表示するプログラムです。
ExplorerへのDLL注入や、ExplorerPatcherは使いません。
元のTClock-Win10プログラムを上書きしません。

## 対応している機能

- 年月日、曜日、時分秒、改行を含む一部の書式
- 文字色、背景色、フォントとサイズのINI設定
- CPU使用率、メモリ使用率、通信量の表示
- CPU使用率の棒メーター
- 時計左クリックで通知センター、ダブルクリックで日時設定
- 右クリックメニューからINIを開く・再読込・終了
- タスクバー再配置とExplorer再起動後のトレイアイコン復元（試作）

## 初回テスト

1. GitHub Actions の「Build TClock11 Standalone (x64)」が成功してから、Artifactをダウンロード。
2. ZIPを展開し、EXEとINIを同じフォルダーに配置。
3. **Windows 11検証環境**で元のTClock-Win10を終了してから試用。
4. 表示位置がずれるときはINIの [Placement] を編集する。
5. 時計または通知領域のアイコンを右クリックし「Reload configuration」で反映。
6. 終了は「Exit TClock11」。

デフォルトの表示書式は yyyy/mm/dd ddd\nhh:nn:ss です。
全機能の移植・Windows 11実機検証は未完了です。
特に標準時計との重なり、マルチモニター、タスクバー自動非表示、
高DPI、全画面アプリ時の表示を確認してください。

INIの [Compatibility] UseOriginalFormat=1 は一部書式だけの実験的機能です。
元の設定ファイルのコピーで試し、オリジナルを上書きしないでください。

ライセンス：GPL-3.0-or-later。元プロジェクトのLICENSEを同梱。
