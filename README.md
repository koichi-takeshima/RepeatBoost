[English](#repeatboost) | [日本語](#japanese)

# RepeatBoost

RepeatBoost is a lightweight system tray utility that provides a shorter Initial Delay and a faster Repeat Interval than the standard keyboard repeat settings in Windows 11.

A short tap is handled as a normal single keystroke. RepeatBoost starts its own repeat only when a key is held down. It prioritizes avoiding unintended input, duplicate repeat, and stuck key state over raw speed.

## System requirements

- Windows 11
- x64
- Product Version: **1.1.0**

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

The tray menu contains:

- **Enable** — turn RepeatBoost's custom key repeat on or off
- **Settings** — open the Settings window
- **Exit** — exit RepeatBoost

When the UI language is Japanese, these items are shown as **有効**, **設定**, and **終了**.

When enabled, **Enable** has a check mark. The enabled / disabled state is saved as a setting and is preserved across restarts.

## Settings

The Settings window has two global controls:

| Item | Values | Default |
|---|---|---|
| Enabled | Enabled / Disabled | Disabled |
| Windows Logon (Autostart) | On / Off | Off |

**Enabled** controls RepeatBoost's custom repeat for all Repeat settings. **Windows Logon** controls whether the Engine starts when you sign in to Windows.

### Repeat settings

Below these global controls, **Repeat settings** contains **1–16 ordered sets**, displayed as **Setting 1**, **Setting 2**, and so on. Each set has its own three settings:

| Setting in each set | Values | Default |
|---|---|---|
| Initial delay | 0–250 ms | 250 ms |
| Repeat interval | 5–33 ms | 33 ms |
| Target keys | Arrow keys only / All keys / Custom | Arrow keys only |

A fresh installation starts **disabled**, with **one set** (250 ms / 33 ms / Arrow keys only) and autostart off. Adding a set creates another set with these same timing and target defaults.

Use the icon buttons to manage sets:

- **+** beside Repeat settings (**Add repeat set**): add a set at the bottom. The button is disabled when there are 16 sets.
- **Up / Down** on a set: move it higher or lower in the list.
- **Delete** on a set: remove it. At least one set must remain, so Delete is disabled when only one is present.

**Order determines priority.** For a newly pressed key, RepeatBoost checks sets from the top and uses the **first set whose Target keys match**. Only that set's Initial delay and Repeat interval apply. If a key is included in more than one set, a lower set does **not** override a higher matching set. If no set matches, RepeatBoost does not start custom repeat for that key.

### Custom target keys

Select **Custom** in a set's **Target keys** card to specify individual repeat targets. The selected keys appear as tokens in a vertical list below the selector.

To add a key, click **+** (**Add custom key**) below the list. When prompted to press a key, **press the physical key you want to register**. The accepted key appears in the list; click a token's **×** to remove it. A key that is not eligible is ignored during capture. You can cancel capture by moving focus away.

You cannot add Ctrl / Shift / Alt / Win or toggle keys such as CapsLock / NumLock / ScrollLock as repeat targets. Other keys, including function, media and keyboard-specific keys, may be used when their physical key event is delivered by Windows. Typing text into the token field does **not** register a key or define a macro. An empty Custom list matches no keys.

### Saving and window layout

Settings has no Save button. Closing the Settings window normally saves the changes that exist at that time.

If saving fails, you can retry, discard the changes and close, or cancel the close operation.

While Settings is open, RepeatBoost continues running with the current settings. When Settings closes, RepeatBoost reloads the saved settings, so restarting RepeatBoost itself is not required.

The Settings window can be resized or maximized. It remembers its normal position and size and whether it was maximized, then restores them when you open it again (adjusting an unreachable position if the monitor layout changes).

The settings file is stored at:

```text
%LOCALAPPDATA%\RepeatBoost\settings.ini
```

## Key repeat behavior

For RepeatBoost target keys, the first physical key press passes through once as usual. RepeatBoost starts its own repeat only when the key remains held down.

Main behavior:

- A short tap produces only one keystroke.
- Holding a key starts repeat after the Initial delay of the highest-priority matching set.
- Even with `Initial delay = 0`, the first custom repeat occurs **after one Repeat interval** of the matching set, not immediately after the first key press.
- When multiple keys are pressed, only the last pressed target key repeats.
- If B is pressed while A is held, A's repeat stops. Releasing B does not automatically resume A while A remains held.
- Pressing another physical key while repeat is active stops the current repeat even if that key is not a repeat target.
- Repeat does not continue after KeyUp.

### Target keys

**Arrow Keys Only**

- The Up / Down / Left / Right arrow keys are repeat targets.

**All Keys**

- Ordinary keys, F1–F12, media keys, OEM / Japanese-keyboard-specific keys, PrintScreen, Pause, and other physical keys can be targets when Windows delivers their key events.
- Modifier keys themselves, such as Ctrl / Shift / Alt / Win, do not start repeat.
- Toggle keys themselves, such as CapsLock / NumLock / ScrollLock, also do not start repeat.

**Custom**

- Only the keys registered in that set's Custom list can match. Use Settings → Target keys → Custom to add keys by pressing them or remove tokens with **×**.
- Modifier and toggle keys are excluded here too. No custom repeat starts for an empty list.

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
- Product Version: **1.1.0**
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
- Product Version: **1.1.0**

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

Settings には、すべてのリピート設定に共通する次の項目があります。

| 項目 | 値 | 既定値 |
|---|---|---|
| 有効 | 有効 / 無効 | 無効 |
| 自動起動（Windows ログオン） | オン / オフ | オフ |

**有効** は RepeatBoost の独自リピート全体を切り替えます。**自動起動** は Windows サインイン時に Engine を起動するかどうかを指定します。

### リピート設定

その下の **リピート設定** には、**1～16 件の設定**を順序付きで登録できます。画面には **設定 1**、**設定 2** … と表示され、各設定には次の 3 項目があります。

| 各設定の項目 | 値 | 既定値 |
|---|---|---|
| 初期遅延 | 0～250 ms | 250 ms |
| リピート間隔 | 5～33 ms | 33 ms |
| 対象キー | 矢印キーのみ / すべてのキー / カスタム | 矢印キーのみ |

初期状態は **無効**、リピート設定が **1 件**（250 ms / 33 ms / 矢印キーのみ）、自動起動がオフです。設定を追加した場合も、追加された設定の初期遅延・リピート間隔・対象キーはこの既定値になります。

設定の操作には次のアイコンを使用します。

- リピート設定の右側の **＋**（**リピート設定を追加**）: 一覧の末尾に追加します。16 件ある場合は追加できません。
- 各設定の **上へ / 下へ**: 一覧内で設定の順序を入れ替えます。
- 各設定の **削除**: その設定を削除します。最低 1 件は必要なため、1 件だけの場合は削除できません。

**一覧の上にある設定ほど優先されます。** キーを新しく押したとき、RepeatBoost は上から順に対象キーを調べ、**最初に一致した設定**の初期遅延とリピート間隔だけを適用します。同じキーを複数の設定に登録しても、下位の設定が上位の一致を上書きすることはありません。どの設定にも一致しないキーでは、独自リピートを開始しません。

### カスタム対象キー

各設定の **対象キー** で **カスタム** を選ぶと、キーを個別に指定できます。登録済みのキーは選択欄の下に縦並びのトークンとして表示されます。

キーを追加するには、一覧の下の **＋**（**カスタムキーを追加**）を押し、キー入力の案内が表示されたら **登録したい物理キーを実際に押します**。受け付けられたキーが一覧に追加されます。削除するときは各トークンの **×** を押します。登録できないキーは入力待ちの間に無視され、フォーカスを移すと入力待ちを解除できます。

Ctrl / Shift / Alt / Win や CapsLock / NumLock / ScrollLock などの切り替えキー自身は登録できません。それ以外のファンクションキー、メディアキー、キーボード固有キーなども、Windows から物理キー入力として通知される場合は対象にできます。トークン欄へ文字を直接入力してもキー登録やマクロの設定にはなりません。カスタムが空の場合は、どのキーにも一致しません。

### 保存とウィンドウ配置

Settings には保存ボタンがありません。通常どおりウィンドウを閉じると、その時点の変更内容を保存します。

保存に失敗した場合は、再試行、変更を破棄して閉じる、または閉じる操作の取り消しを選べます。

Settings を開いている間も RepeatBoost は現在の設定で動作を続けます。Settings を閉じると保存済みの設定を再読み込みするため、RepeatBoost 本体の再起動は不要です。

Settings のウィンドウはサイズ変更と最大化に対応します。通常時の位置・サイズと最大化状態が記録され、次回起動時に復元されます。モニター構成が変わって画面外になる場合は位置が補正されます。

設定ファイルの保存先:

```text
%LOCALAPPDATA%\RepeatBoost\settings.ini
```

## キーリピート動作

RepeatBoost の対象キーでは、最初の物理キー押下を通常どおり1回通し、キーを押し続けた場合だけ独自リピートを開始します。

主な動作:

- 短いタップは1打だけ
- キーを保持すると、一致した最優先設定の初期遅延の経過後にリピートを開始
- `初期遅延 = 0` でも、最初の独自リピートはキー押下直後ではなく、一致した設定の **1 リピート間隔後**
- 複数キーを押した場合は、最後に押した対象キーだけをリピート
- Aを保持中にBを押すとAのリピートを停止し、Bを離しても保持中のAは自動再開しない
- リピート中に別の物理キーを押すと、そのキーがリピート対象外でも現在のリピートを停止
- KeyUp 後にリピートを継続しない

### 対象キー

**矢印キーのみ**

- 上 / 下 / 左 / 右の矢印キーをリピート対象にします。

**すべてのキー**

- 通常キー、F1～F12、メディアキー、OEM / 日本語キーボード固有キー、PrintScreen、Pause なども、Windows から物理キー入力として通知される場合は対象になります。
- Ctrl / Shift / Alt / Win 等の modifier key 自身はリピートを開始しません。
- CapsLock / NumLock / ScrollLock 等の切り替えキー自身もリピートを開始しません。

**カスタム**

- その設定のカスタム一覧に登録されたキーだけが対象です。Settings の「対象キー → カスタム」で物理キーを押して追加し、トークンの **×** で削除します。
- ここでも修飾キーと切り替えキーは除外されます。登録が空の場合は独自リピートを開始しません。

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
- Product Version: **1.1.0**
- Author / Publisher: **Koichi Takeshima**

## License

RepeatBoost は [MIT License](LICENSE) です。

Copyright (c) 2026 Koichi Takeshima
