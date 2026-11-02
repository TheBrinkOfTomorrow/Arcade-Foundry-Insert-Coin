import SwiftUI

struct CabinetList: Identifiable {
    let id = UUID()
    let cabinets: [SaveSync.CabinetInfo]
}

/// On a device new to Arcade Foundry: Insert Coin: carry on as a cabinet already in
/// iCloud (the device this one replaces), or start as a new one.
struct ContinueAsView: View {
    let cabinets: [SaveSync.CabinetInfo]
    var done: () -> Void
    @State private var busy = false
    @State private var problem: String?

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Text("Your iCloud already has these cabinets. If this device replaces one of them, continue as it to get its saves back: its settings, link role and records.")
                        .font(.callout)
                }
                Section("Continue as") {
                    ForEach(cabinets) { c in
                        Button {
                            busy = true
                            Task {
                                do {
                                    try await SaveSync.adopt(c)
                                    done()
                                } catch {
                                    problem = error.localizedDescription
                                }
                                busy = false
                            }
                        } label: {
                            CabinetRow(cabinet: c)
                        }
                    }
                }
                if let problem {
                    Text(problem).font(.caption).foregroundStyle(.red)
                }
            }
            .formStyle(.grouped)
            .disabled(busy)
            .navigationTitle("This cabinet")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("New cabinet") {
                        Task { await SaveSync.announce() }
                        done()
                    }
                }
            }
        }
        #if os(macOS)
        .frame(minWidth: 440, minHeight: 360)
        #endif
    }
}

/// A cabinet's ID: the same on two devices means they are one cabinet.
struct CabinetIDRow: View {
    let id: String

    var body: some View {
        LabeledContent("ID") {
            Text(id).font(.caption.monospaced())
            #if !os(tvOS)
                .textSelection(.enabled)
            #endif
        }
    }
}

struct CabinetRow: View {
    let cabinet: SaveSync.CabinetInfo

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(cabinet.name).font(.headline)
            Text(detail).font(.caption).foregroundStyle(.secondary)
            Text("ID \(cabinet.id)").font(.caption2.monospaced()).foregroundStyle(.secondary)
        }
    }

    private var detail: String {
        var parts = [cabinet.model]
        if let seen = cabinet.lastSeen { parts.append("last used \(seen.formatted(date: .abbreviated, time: .shortened))") }
        let games = cabinet.games.keys.compactMap { id in GameInfo.all.first { $0.id == id }?.title }.sorted()
        parts.append(games.isEmpty ? "no saves" : games.joined(separator: ", "))
        return parts.joined(separator: " · ")
    }
}

/// Settings: this cabinet's name, and the other cabinets in iCloud. The
/// settings Form presents CabinetsView for `showCabinets`: a sheet on a row
/// of the Form is torn down with the row on iOS and tvOS, and takes the
/// settings sheet with it.
struct CabinetSettingsSection: View {
    @Binding var showCabinets: Bool
    @State private var name = Cabinet.name
    @State private var id = Cabinet.id
    @State private var confirmNew = false
    @State private var renewing = false

    var body: some View {
        Section("This cabinet") {
            TextField("Name", text: $name, prompt: Text(Cabinet.model))
                .onSubmit(rename)
                .onDisappear(perform: rename)
            CabinetIDRow(id: id)
            Button("Other cabinets in iCloud…") { showCabinets = true }
            Button(renewing ? "Making a new cabinet…" : "Make this a new cabinet…") { confirmNew = true }
                .disabled(renewing)
            Text("Each device is a cabinet of its own. Its games' saves (settings, link role, records) are backed up to iCloud when you leave a game (or once it's back online), and a game starts from the newest backup, so devices sharing a cabinet carry on from each other. Cabinets don't share saves, so linked cabinets keep their roles.")
                .font(.caption).foregroundStyle(.secondary)
        }
        .confirmationDialog("Make this a new cabinet?", isPresented: $confirmNew, titleVisibility: .visible) {
            Button("Make a new cabinet") {
                rename()
                renewing = true
                Task {
                    await SaveSync.becomeNew()
                    id = Cabinet.id
                    renewing = false
                }
            }
        } message: {
            Text("This device gets a cabinet ID of its own and backs up its game saves under it, so it no longer shares a cabinet with a device it was transferred from. The other device keeps the old cabinet and its backups. Give this one a name of its own above too.")
        }
        // A transfer in CabinetsView changes this cabinet's name and ID.
        .onChange(of: showCabinets) { shown in
            guard !shown else { return }
            name = Cabinet.name
            id = Cabinet.id
        }
    }

    private func rename() {
        let n = name.trimmingCharacters(in: .whitespaces)
        guard !n.isEmpty, n != Cabinet.name else { return }
        Cabinet.name = n
        LinkDiscovery.shared.advertise()
        Task { await SaveSync.announce() }
    }
}

/// The other cabinets: continue as one, copy a game's save from one, or
/// delete one from iCloud.
struct CabinetsView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var cabinets: [SaveSync.CabinetInfo]?
    @State private var problem: String?
    @State private var busy = false
    @State private var confirmAdopt: SaveSync.CabinetInfo?
    @State private var confirmDelete: SaveSync.CabinetInfo?

    var body: some View {
        NavigationStack {
            Form {
                if let cabinets {
                    if cabinets.isEmpty {
                        Text("No other cabinets in iCloud.").foregroundStyle(.secondary)
                    }
                    ForEach(cabinets) { c in
                        Section {
                            CabinetRow(cabinet: c)
                        }
                        Section {
                            if c.games.isEmpty {
                                Text("No game saves backed up yet. A cabinet backs up a game's save (its settings, link role and records) when you leave the game on it.")
                                    .font(.caption).foregroundStyle(.secondary)
                            }
                            ForEach(c.games.sorted { title($0.key) < title($1.key) }, id: \.key) { save in
                                Button {
                                    run { try await SaveSync.copy(save.key, from: c) }
                                } label: {
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text("Copy its \(title(save.key)) save to this cabinet")
                                        Text("Backed up \(save.value.formatted(date: .abbreviated, time: .shortened))")
                                            .font(.caption).foregroundStyle(.secondary)
                                    }
                                }
                            }
                        } header: {
                            Text("\(c.name)'s game saves")
                        }
                        Section {
                            Button("Transfer its name and game saves to this device…") { confirmAdopt = c }
                            Button("Delete from iCloud", role: .destructive) { confirmDelete = c }
                        }
                    }
                    if !cabinets.isEmpty {
                        Text("A copied save brings that cabinet's link role with it: set it again in the game's test mode if both cabinets link.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                } else if problem == nil {
                    ProgressView()
                }
                if let problem {
                    Text(problem).font(.caption).foregroundStyle(.red)
                }
            }
            .formStyle(.grouped)
            .disabled(busy)
            .navigationTitle("Cabinets")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } }
            }
            .task { await load() }
            .confirmationDialog("Transfer \(confirmAdopt?.name ?? "") to this device?", isPresented: Binding(get: { confirmAdopt != nil }, set: { if !$0 { confirmAdopt = nil } }), titleVisibility: .visible) {
                Button("Transfer") {
                    if let c = confirmAdopt { run { try await SaveSync.adopt(c) } }
                }
            } message: {
                Text("This device becomes that cabinet: it takes its name and all its game saves (settings, link role and records), replacing this device's. App settings stay as they are. Both devices are then the same cabinet and share one backup, so use this when this device replaces that one; to bring over one game's save, copy it instead.")
            }
            .confirmationDialog("Delete \(confirmDelete?.name ?? "") from iCloud?", isPresented: Binding(get: { confirmDelete != nil }, set: { if !$0 { confirmDelete = nil } }), titleVisibility: .visible) {
                Button("Delete", role: .destructive) {
                    if let c = confirmDelete { run { try await SaveSync.delete(c) } }
                }
            } message: {
                Text("Its saves backup is deleted. The device itself keeps its saves, and backs them up again if it is still used.")
            }
        }
        #if os(macOS)
        .frame(minWidth: 480, minHeight: 420)
        #endif
    }

    private func title(_ romset: String) -> String {
        GameInfo.all.first { $0.id == romset }.map { "\($0.title) (\(romset))" } ?? romset
    }

    private func load() async {
        guard await SaveSync.available() else {
            problem = "iCloud is not available: sign in to iCloud on this device."
            return
        }
        do {
            cabinets = try await SaveSync.cabinets().filter { $0.id != Cabinet.id }
        } catch {
            problem = error.localizedDescription
        }
    }

    private func run(_ action: @escaping () async throws -> Void) {
        busy = true
        problem = nil
        Task {
            do {
                try await action()
                await load()
            } catch {
                problem = error.localizedDescription
            }
            busy = false
        }
    }
}
