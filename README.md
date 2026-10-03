[English](#repeatboost) | [日本語](#japanese)

# RepeatBoost

RepeatBoost is a lightweight system tray utility that provides a shorter Initial Delay and a faster Repeat Interval than the standard keyboard repeat settings in Windows 11.

A short tap is handled as a normal single keystroke. RepeatBoost starts its own repeat only when a key is held down. It prioritizes avoiding unintended input, duplicate repeat, and stuck key state over raw speed.

## System requirements

- Windows 11
- x64
- Product Version: **1.0.0**

## Installation

Install RepeatBoost by running the distributed RepeatBoost Setup. RepeatBoost itself is installed per user, so the normal application installation does not require administrator privileges.

Setup checks the required shared runtimes and installs only the missing ones using the official Microsoft installers. UAC / elevation occurs only when required to install a missing runtime.

RepeatBoost uses the following shared runtimes:

- .NET Runtime x64 10.0.x
- Windows App Runtime stable major 2 / 2.5.1.0 or later
- Visual C++ v14 x64 Runtime

If the required runtimes cannot be satisfied, Setup does not report the RepeatBoost installation as successful.

## Startup and system tray

When RepeatBoost starts, the RepeatBoost icon appears in the Windows notification area.

On Japanese environments, the tray menu contains:

- **有効** — enable / disable RepeatBoost's custom repeat
- **設定** — open the Settings window
- **終了** — exit RepeatBoost

On English environments, these items are shown as `Enable` / `Settings` / `Exit`.

When enabled, **有効 / Enable** has a check mark. The enabled / disabled state is saved as a setting and is preserved across restarts.

## Settings

The following settings can be changed:

| Item | Values | Default |
|---|---|---|
| Enabled | Enabled / Disabled | Disabled |
| Initial Delay | 0–250 ms | 250 ms |
| Repeat Interval | 5–33 ms | 33 ms |
| Target Keys | Arrow Keys Only / All Keys | Arrow Keys Only |
| Windows Logon | On / Off | Off |

Settings has no Save button. Closing the Settings window normally saves the changes that exist at that time.

If saving fails, you can retry, discard the changes and close, or cancel the close operation.

While Settings is open, RepeatBoost continues running with the current settings. When Settings closes, RepeatBoost reloads the saved settings, so restarting RepeatBoost itself is not required.

The settings file is stored at:

```text
%LOCALAPPDATA%\RepeatBoost\settings.ini
```

## Key repeat behavior

For RepeatBoost target keys, the first physical key press passes through once as usual. RepeatBoost starts its own repeat only when the key remains held down.

Main behavior:

- A short tap produces only one keystroke.
- Holding a key starts repeat after the Initial Delay.
- Even with `Initial Delay = 0`, the first custom repeat occurs **after one Repeat Interval**, not immediately after the first key press.
- When multiple keys are pressed, only the last pressed target key repeats.
- If B is pressed while A is held, A's repeat stops. Releasing B does not automatically resume A while A remains held.
- Pressing another physical key while repeat is active stops the current repeat even if that key is not a repeat target.
- Repeat does not continue after KeyUp.

### Target keys

**Arrow Keys Only**

- The Up / Down / Left / Right arrow keys are repeat targets.

**All Keys**

- Ordinary keys, F1–F12, media keys, OEM / Japanese-keyboard-specific keys, PrintScreen, Pause, and similar keys can be repeat targets.
- Modifier keys themselves, such as Ctrl / Shift / Alt / Win, do not start repeat.
- Toggle keys themselves, such as CapsLock / NumLock / ScrollLock, also do not start repeat.

## Start at Windows logon

Turning on **Windows Logon** in Settings starts the RepeatBoost Engine automatically when you sign in to Windows.

The Settings application itself does not start automatically.

## Upgrade

Running a newer RepeatBoost Setup upgrades the existing RepeatBoost installation.

- If RepeatBoost was running when the upgrade started, RepeatBoost is restarted after a successful upgrade.
- If RepeatBoost was stopped when the upgrade started, the upgrade alone does not start it automatically.
- `%LOCALAPPDATA%\RepeatBoost\settings.ini` is preserved.

## Uninstall

RepeatBoost can be uninstalled from:

```text
Settings > Apps > Installed apps > RepeatBoost
```

Uninstall removes RepeatBoost itself and the registration information created by the installer, but does not remove:

- `%LOCALAPPDATA%\RepeatBoost\settings.ini`
- shared .NET Runtime
- shared Windows App Runtime
- shared Visual C++ Runtime

A previous `settings.ini` can be reused after reinstalling RepeatBoost.

## Limitations / notes

- Windows 11 / x64 only.
- Custom repeat is not guaranteed for applications running at a higher integrity level than RepeatBoost, such as applications running with administrator privileges.
- The minimum Repeat Interval is 5 ms, but this is not an SLA guaranteeing exactly 5.000 ms of wall-clock time on every repeat. Actual timing is affected by Windows, the target application, system load, and other factors.

## Version / Author

- Product: **RepeatBoost**
- Product Version: **1.0.0**
- Author / Publisher: **Koichi Takeshima**

## License

RepeatBoost is distributed under the [MIT License](LICENSE).

Copyright (c) 2026 Koichi Takeshima

---

<a id="japanese"></a>
# RepeatBoost

RepeatBoost は、Windows 11 の標準 keyboard repeat より短い Initial Delay / 高速な Repeat Interval を利用できる、軽量なタスクトレイ常駐型 utility です。

短いタップは通常どおり1打として扱い、キーを押し続けた場合だけ RepeatBoost 独自のリピートを開始します。速度よりも、誤入力・二重リピート・キー状態の残留を避けることを優先しています。

## 動作環境

- Windows 11
- x64
- Product Version: **1.0.0**

## インストール

配布される RepeatBoost Setup を実行してインストールします。RepeatBoost 本体はユーザー単位で導入されるため、通常の本体インストール自体には管理者権限を必要としません。

Setup は必要な共有ランタイムを確認し、不足しているものだけ Microsoft official installer から導入します。不足しているランタイムの導入時だけ、必要に応じて UAC / elevation が発生します。

RepeatBoost が利用する共有ランタイムは次のとおりです。

- .NET Runtime x64 10.0.x
- Windows App Runtime stable major 2 / 2.5.1.0 以上
- Visual C++ v14 x64 Runtime

必要なランタイムを満たせない場合は、RepeatBoost のインストールを成功扱いにしません。

## 起動とタスクトレイ

RepeatBoost を起動すると Windows の通知領域に RepeatBoost icon が表示されます。

日本語環境の tray menu:

- **有効** — RepeatBoost の独自リピートを有効 / 無効にする
- **設定** — Settings window を開く
- **終了** — RepeatBoost を終了する

英語環境では `Enable` / `Settings` / `Exit` と表示されます。

有効時は **有効 / Enable** に check が付きます。有効 / 無効の状態は設定として保存され、次回起動時にも引き継がれます。

## Settings

Settings では次の項目を変更できます。

| 項目 | 値 | 既定値 |
|---|---|---|
| 有効 | 有効 / 無効 | 無効 |
| 初期遅延 | 0–250 ms | 250 ms |
| リピート間隔 | 5–33 ms | 33 ms |
| 対象キー | 矢印キーのみ / すべてのキー | 矢印キーのみ |
| Windows ログオン | オン / オフ | オフ |

Settings には Save button はありません。通常どおり Settings window を閉じると、その時点の変更内容を保存します。

保存に失敗した場合は、再試行、変更を破棄して閉じる、または閉じる操作を取り消すことができます。

Settings を開いている間も RepeatBoost は現在の設定で動作を継続します。Settings を閉じると保存済みの設定を再読み込みするため、RepeatBoost 本体の再起動は不要です。

設定ファイルは次に保存されます。

```text
%LOCALAPPDATA%\RepeatBoost\settings.ini
```

## キーリピート動作

RepeatBoost の対象キーでは、最初の物理キー押下を通常どおり1回通し、キーを押し続けた場合だけ独自リピートを開始します。

主な動作:

- 短いタップは1打だけ
- キーを保持すると Initial Delay 経過後にリピートを開始
- `Initial Delay = 0` でも、最初の独自リピートは最初のキー押下直後ではなく **1 Repeat Interval 後**
- 複数キーを押した場合は、最後に押した対象キーだけをリピート
- Aを保持中にBを押すとAのリピートを停止し、Bを離しても保持中のAは自動再開しない
- リピート中に別の物理キーを押すと、そのキーがリピート対象外でも現在のリピートを停止
- KeyUp 後にリピートを継続しない

### 対象キー

**矢印キーのみ**

- 上 / 下 / 左 / 右の矢印キーをリピート対象にします。

**すべてのキー**

- 通常キー、F1–F12、media key、OEM / 日本語キーボード固有 key、PrintScreen、Pause 等を対象にできます。
- Ctrl / Shift / Alt / Win 等の modifier key 自身はリピートを開始しません。
- CapsLock / NumLock / ScrollLock 等の toggle key 自身もリピートを開始しません。

## Windows ログオン時の自動起動

Settings の **Windows ログオン** をオンにすると、Windows へのサインイン時に RepeatBoost Engine を自動起動します。

Settings application 自体は自動起動しません。

## Upgrade

新しい RepeatBoost Setup を実行すると、同じ RepeatBoost installation として upgrade します。

- upgrade 開始時に RepeatBoost が実行中だった場合は、upgrade 成功後に RepeatBoost を再起動します。
- upgrade 開始時に RepeatBoost が停止していた場合は、upgrade だけを理由に自動起動しません。
- `%LOCALAPPDATA%\RepeatBoost\settings.ini` は保持します。

## アンインストール

Windows の次の画面から RepeatBoost をアンインストールできます。

```text
設定 > アプリ > インストールされているアプリ > RepeatBoost
```

アンインストールでは RepeatBoost 本体とインストーラーが作成した登録情報を削除しますが、次は削除しません。

- `%LOCALAPPDATA%\RepeatBoost\settings.ini`
- shared .NET Runtime
- shared Windows App Runtime
- shared Visual C++ Runtime

再インストール時にも以前の `settings.ini` を利用できます。

## 制限 / 注意事項

- Windows 11 / x64 のみ対応します。
- RepeatBoost より高い権限（管理者権限など）で動作する application への独自リピートは保証しません。
- Repeat Interval の下限は 5 ms ですが、これは wall-clock で常に exactly 5.000 ms を保証する SLA ではありません。実際の timing は Windows、対象 application、system load 等の影響を受けます。

## Version / Author

- Product: **RepeatBoost**
- Product Version: **1.0.0**
- Author / Publisher: **Koichi Takeshima**

## License

RepeatBoost は [MIT License](LICENSE) です。

Copyright (c) 2026 Koichi Takeshima
