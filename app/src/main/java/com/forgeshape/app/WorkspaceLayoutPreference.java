package com.forgeshape.app;

/**
 * Which workspace LAYOUT the user asked for (`WORKBENCH-RIBBON-R2`, OWNER
 * 2026-11-05: "it has to be possible to switch the view on Android from the
 * PC version — what it looks like on a tablet — to the small smartphone
 * version").
 *
 * <p>An {@link AppPreferences} field, so application state and never project
 * truth: it reaches no {@code .forge} byte, moves no fingerprint and records
 * no history step. It is read by {@link WorkspaceLayoutMode#forWindow(int,
 * int, WorkspaceLayoutPreference)} on every window measure, so changing it is
 * exactly a resize.
 *
 * <ul>
 *   <li>{@link #AUTOMATIC} — the window arithmetic decides, as it always has:
 *       a phone is compact, a tablet in landscape is the workbench.</li>
 *   <li>{@link #PHONE} — the compact composition whatever the window: the
 *       floating capsules, the bottom sheet, no docked panels, no ribbon.</li>
 *   <li>{@link #WORKBENCH} — the desktop's arrangement wherever the window is
 *       at least a medium one: the ribbon, the docked Properties panel and,
 *       where the width affords it, the Browser. A window too narrow to
 *       carry a workbench is still laid out by the arithmetic, because a
 *       preference cannot make 400 dp hold three columns.</li>
 * </ul>
 */
enum WorkspaceLayoutPreference {

    AUTOMATIC,

    PHONE,

    WORKBENCH;

    static WorkspaceLayoutPreference defaultPreference() {
        return AUTOMATIC;
    }

    /** The stored name's value, or the default for a name this build does not know. */
    static WorkspaceLayoutPreference fromStoredName(String name) {
        for (WorkspaceLayoutPreference candidate : values()) {
            if (candidate.name().equals(name)) {
                return candidate;
            }
        }
        return defaultPreference();
    }

    int labelRes() {
        switch (this) {
            case PHONE:
                return R.string.layout_phone;
            case WORKBENCH:
                return R.string.layout_workbench;
            default:
                return R.string.layout_automatic;
        }
    }
}
