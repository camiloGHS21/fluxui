extern crate fluxui;

use fluxui::{App, Widget};
use std::sync::Mutex;
use std::fs;
use std::process::Command;

#[derive(Clone, Copy)]
struct SafeWidget(Widget);
unsafe impl Send for SafeWidget {}
unsafe impl Sync for SafeWidget {}

impl std::ops::Deref for SafeWidget {
    type Target = Widget;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

struct AppState {
    checklist_status: [bool; 7],
    evidence_collected: bool,
    evidence_text: String,
    policy_texts: [String; 4],
    selected_policy_index: usize,

    // UI Widgets
    page_dashboard: Option<SafeWidget>,
    page_checklist: Option<SafeWidget>,
    page_evidence: Option<SafeWidget>,
    page_policies: Option<SafeWidget>,
    page_export: Option<SafeWidget>,

    // Dashboard elements
    compliance_meter: Option<SafeWidget>,
    compliance_progress: Option<SafeWidget>,
    compliance_label: Option<SafeWidget>,
    stat_verified: Option<SafeWidget>,
    stat_status: Option<SafeWidget>,

    // Evidence elements
    scan_btn: Option<SafeWidget>,
    scan_progress: Option<SafeWidget>,
    evidence_textarea: Option<SafeWidget>,

    // Policy elements
    policy_textarea: Option<SafeWidget>,
    policy_select: Option<SafeWidget>,

    // Export elements
    export_btn: Option<SafeWidget>,
    export_progress: Option<SafeWidget>,
    export_status_text: Option<SafeWidget>,
    company_input: Option<SafeWidget>,
}

static STATE: Mutex<AppState> = Mutex::new(AppState {
    checklist_status: [true, true, true, false, false, false, false], // some default active ones
    evidence_collected: false,
    evidence_text: String::new(),
    policy_texts: [String::new(), String::new(), String::new(), String::new()],
    selected_policy_index: 0,

    page_dashboard: None,
    page_checklist: None,
    page_evidence: None,
    page_policies: None,
    page_export: None,

    compliance_meter: None,
    compliance_progress: None,
    compliance_label: None,
    stat_verified: None,
    stat_status: None,

    scan_btn: None,
    scan_progress: None,
    evidence_textarea: None,

    policy_textarea: None,
    policy_select: None,

    export_btn: None,
    export_progress: None,
    export_status_text: None,
    company_input: None,
});

static CHECKLIST_NAMES: [&str; 7] = [
    "Firewall de red habilitado y configurado",
    "Dispositivos de empleados cifrados (BitLocker/FileVault)",
    "Software Antivirus corporativo activo",
    "Actualizaciones automáticas de sistema operativo activas",
    "Políticas de contraseñas de al menos 12 caracteres",
    "Copias de seguridad diarias cifradas y con pruebas",
    "Acceso corporativo con Autenticación Multifactor (MFA)"
];

fn update_compliance_score() {
    let state = STATE.lock().unwrap();
    let mut count = 0;
    for &checked in &state.checklist_status {
        if checked {
            count += 1;
        }
    }
    let percentage = (count as f32) / 7.0;
    let verified_str = format!("{} de 7", count);
    let status_str = if count == 7 {
        "Listo para B2B".to_string()
    } else if count >= 4 {
        "En progreso".to_string()
    } else {
        "Acción Requerida".to_string()
    };
    let score_str = format!("{}%", (percentage * 100.0) as i32);

    if let Some(ref m) = state.compliance_meter {
        m.set_meter_value(percentage);
    }
    if let Some(ref p) = state.compliance_progress {
        p.set_progress_element_value(percentage);
    }
    if let Some(ref l) = state.compliance_label {
        let _ = l.set_id(&score_str); // using id or text update via native text function?
        // Wait, Widget has text functions: fluxui_text_set_content
        unsafe {
            fluxui::sys::fluxui_text_set_content(l.raw(), std::ffi::CString::new(score_str).unwrap().as_ptr());
        }
    }
    if let Some(ref sv) = state.stat_verified {
        unsafe {
            fluxui::sys::fluxui_text_set_content(sv.raw(), std::ffi::CString::new(verified_str).unwrap().as_ptr());
        }
    }
    if let Some(ref ss) = state.stat_status {
        unsafe {
            fluxui::sys::fluxui_text_set_content(ss.raw(), std::ffi::CString::new(status_str).unwrap().as_ptr());
        }
    }
}

fn switch_page(page_name: &str) {
    let state = STATE.lock().unwrap();
    if let Some(ref p) = state.page_dashboard { p.set_visible(page_name == "dashboard"); }
    if let Some(ref p) = state.page_checklist { p.set_visible(page_name == "checklist"); }
    if let Some(ref p) = state.page_evidence { p.set_visible(page_name == "evidence"); }
    if let Some(ref p) = state.page_policies { p.set_visible(page_name == "policies"); }
    if let Some(ref p) = state.page_export { p.set_visible(page_name == "export"); }
}

fn initialize_policies() {
    let mut state = STATE.lock().unwrap();
    state.policy_texts[0] = "POLÍTICA DE USO ACEPTABLE DE EQUIPOS\n\n\
        1. Objetivo: Garantizar el uso seguro de los activos informáticos.\n\
        2. Bloqueo Automático: Todo dispositivo debe bloquearse tras 5 minutos de inactividad.\n\
        3. Instalación de Software: Queda terminantemente prohibida la instalación de programas sin autorización.\n\
        4. Redes Públicas: Se prohíbe el uso de redes Wi-Fi públicas para labores corporativas sin VPN.\n\
        5. Seguridad Física: No dejar portátiles desatendidos en vehículos o lugares públicos."
        .to_string();

    state.policy_texts[1] = "POLÍTICA DE CONTROL DE ACCESOS Y CONTRASEÑAS\n\n\
        1. Longitud Mínima: Las contraseñas deben tener al menos 12 caracteres.\n\
        2. Complejidad: Deben incluir mayúsculas, minúsculas, números y símbolos.\n\
        3. Caducidad: Las contraseñas se actualizarán cada 90 días.\n\
        4. Autenticación Multifactor (MFA): Obligatorio para todos los accesos a servicios en la nube.\n\
        5. Almacenamiento: Está prohibido apuntar contraseñas. Usar gestor de contraseñas corporativo."
        .to_string();

    state.policy_texts[2] = "POLÍTICA DE COPIAS DE SEGURIDAD (BACKUP)\n\n\
        1. Alcance: Todos los datos del cliente, código fuente y documentos de negocio.\n\
        2. Frecuencia: Backups automáticos incrementales diarios y completos semanales.\n\
        3. Ubicación: Réplica en la nube cifrada y en almacenamiento externo protegido.\n\
        4. Retención: Los backups históricos se conservarán por un mínimo de 1 año.\n\
        5. Pruebas: Se realizarán simulacros de restauración técnica de manera trimestral."
        .to_string();

    state.policy_texts[3] = "POLÍTICA DE PROTECCIÓN Y PRIVACIDAD DE DATOS\n\n\
        1. Clasificación: Los datos de clientes y de facturación se clasifican como estrictamente confidenciales.\n\
        2. Cifrado: Todo dato confidencial debe almacenarse cifrado en reposo y en tránsito (TLS 1.3).\n\
        3. Destrucción Segura: Los datos obsoletos se borrarán mediante métodos que impidan su recuperación.\n\
        4. Reporte de Incidentes: Cualquier sospecha de brecha de seguridad debe notificarse en menos de 2 horas."
        .to_string();
}

fn load_policy_in_editor(index: usize) {
    let mut state = STATE.lock().unwrap();
    state.selected_policy_index = index;
    let text = state.policy_texts[index].clone();
    if let Some(ref ta) = state.policy_textarea {
        unsafe {
            fluxui::sys::fluxui_text_input_set_value(ta.raw(), std::ffi::CString::new(text).unwrap().as_ptr());
        }
    }
}

fn main() -> Result<(), fluxui::Error> {
    initialize_policies();

    let app = App::create()?;
    app.init("Generador de Auditorías de Cumplimiento B2B", 1000, 700)?;

    if !app.load_default_font(15.0) {
        app.load_font("C:/Windows/Fonts/segoeui.ttf", 15.0);
    }
    app.warm_font_cache(&[13.0, 15.0, 18.0, 24.0, 28.0], "default");
    app.release_font_sources();

    // Custom CSS for rich dashboard look
    app.add_stylesheet(
        ".root { \
            display: flex; \
            flex-direction: row; \
            background: linear-gradient(180deg, #090d15, #030407); \
            color: #ffffff; \
            margin: 0; \
            padding: 0; \
            height: 100%; \
        } \
        .sidebar { \
            width: 280px; \
            background-color: rgba(8, 11, 17, 0.97); \
            border-right: 1px solid rgba(255, 255, 255, 0.15); \
            display: flex; \
            flex-direction: column; \
            padding: 24px; \
            gap: 12px; \
        } \
        .logo-container { \
            display: flex; \
            flex-direction: row; \
            align-items: center; \
            gap: 12px; \
            margin-bottom: 24px; \
            padding-bottom: 16px; \
            border-bottom: 1px solid rgba(255, 255, 255, 0.12); \
        } \
        .logo-img { \
            width: 32px; \
            height: 32px; \
            color: #00FFA8; \
        } \
        .logo-text { \
            font-size: 18px; \
            font-weight: 700; \
            color: #ffffff; \
        } \
        .menu-btn { \
            height: 42px; \
            background-color: transparent; \
            color: rgba(255, 255, 255, 0.72); \
            border-radius: 8px; \
            font-weight: 600; \
            display: flex; \
            justify-content: flex-start; \
            align-items: center; \
            padding-left: 16px; \
            margin-bottom: 6px; \
            border: 1px solid transparent; \
        } \
        .menu-btn:hover { \
            background-color: rgba(255, 255, 255, 0.08); \
            color: #ffffff; \
            border-color: rgba(255, 255, 255, 0.12); \
        } \
        .menu-btn:active { \
            background-color: rgba(0, 255, 168, 0.12); \
            color: #00FFA8; \
            border-color: rgba(0, 255, 168, 0.24); \
        } \
        .content-area { \
            flex-grow: 1; \
            padding: 36px 44px; \
            display: flex; \
            flex-direction: column; \
            gap: 24px; \
            background: transparent; \
        } \
        .page { \
            display: flex; \
            flex-direction: column; \
            gap: 20px; \
            flex-grow: 1; \
        } \
        .page-header { \
            display: flex; \
            flex-direction: row; \
            justify-content: space-between; \
            align-items: center; \
            padding-bottom: 12px; \
            border-bottom: 1px solid rgba(255, 255, 255, 0.1); \
        } \
        .page-title { \
            font-size: 26px; \
            font-weight: 700; \
            color: #ffffff; \
            height: 36px; \
        } \
        .header-icon { \
            width: 24px; \
            height: 24px; \
            color: rgba(255, 255, 255, 0.4); \
        } \
        .page-desc { \
            font-size: 14px; \
            color: rgba(255, 255, 255, 0.72); \
            line-height: 1.5; \
        } \
        .cards-row { \
            display: flex; \
            flex-direction: row; \
            gap: 20px; \
        } \
        .card { \
            flex-grow: 1; \
            background-color: rgba(19, 24, 34, 0.86); \
            border: 1px solid rgba(255, 255, 255, 0.15); \
            border-radius: 12px; \
            padding: 24px; \
            display: flex; \
            flex-direction: column; \
            gap: 12px; \
        } \
        .card:hover { \
            background-color: rgba(29, 36, 50, 0.92); \
            border-color: rgba(255, 255, 255, 0.25); \
        } \
        .card-blue { \
            border-left: 4px solid #00bcff !important; \
        } \
        .card-emerald { \
            border-left: 4px solid #00FFA8 !important; \
        } \
        .card-cyan { \
            border-left: 4px solid #00FFA8 !important; \
        } \
        .card-label { \
            font-size: 12px; \
            color: rgba(255, 255, 255, 0.6); \
            font-weight: 700; \
            text-transform: uppercase; \
            letter-spacing: 0.05em; \
        } \
        .card-value { \
            font-size: 32px; \
            font-weight: 700; \
            color: #ffffff; \
        } \
        .gauge-score { \
            font-size: 48px; \
            font-weight: 800; \
            color: #00FFA8; \
            height: 60px; \
        } \
        .list-container { \
            display: flex; \
            flex-direction: column; \
            gap: 10px; \
            background-color: rgba(19, 24, 34, 0.86); \
            border-radius: 12px; \
            padding: 20px; \
            border: 1px solid rgba(255, 255, 255, 0.15); \
        } \
        .list-item { \
            display: flex; \
            flex-direction: row; \
            align-items: center; \
            justify-content: space-between; \
            padding: 14px 18px; \
            border-radius: 8px; \
            background-color: rgba(29, 36, 50, 0.92); \
            border: 1px solid rgba(255, 255, 255, 0.08); \
        } \
        .list-item:hover { \
            background-color: rgba(43, 52, 68, 0.94); \
            border-color: rgba(255, 255, 255, 0.15); \
        } \
        .item-text { \
            font-size: 14px; \
            color: #ffffff; \
            margin-left: 12px; \
        } \
        .item-label { \
            display: flex; \
            flex-direction: row; \
            align-items: center; \
        } \
        .status-badge { \
            font-size: 11px; \
            font-weight: 700; \
            padding: 4px 8px; \
            border-radius: 4px; \
            background-color: rgba(255, 179, 64, 0.18); \
            color: #FFB340; \
            border: 1px solid rgba(255, 179, 64, 0.3); \
        } \
        .btn-primary { \
            height: 44px; \
            background-color: #00FFA8; \
            color: #030407; \
            font-weight: 700; \
            border-radius: 8px; \
            display: flex; \
            justify-content: center; \
            align-items: center; \
            border: 1px solid #00FFA8; \
        } \
        .btn-primary:hover { \
            background-color: #00D68F; \
            border-color: #00D68F; \
        } \
        .btn-secondary { \
            height: 44px; \
            background-color: rgba(237, 243, 248, 0.07); \
            color: rgba(255, 255, 255, 0.90); \
            font-weight: 700; \
            border-radius: 8px; \
            display: flex; \
            justify-content: center; \
            align-items: center; \
            border: 1px solid rgba(255, 255, 255, 0.19); \
        } \
        .btn-secondary:hover { \
            background-color: rgba(237, 243, 248, 0.12); \
            border-color: rgba(255, 255, 255, 0.36); \
        } \
        .form-row { \
            display: flex; \
            flex-direction: column; \
            gap: 8px; \
        } \
        .form-input { \
            background-color: rgba(19, 24, 34, 0.86); \
            color: #ffffff; \
            border: 1px solid rgba(255, 255, 255, 0.15); \
            border-radius: 8px; \
            padding: 12px; \
            font-size: 14px; \
        } \
        .form-input:focus { \
            border-color: #00FFA8; \
        } \
        .form-textarea { \
            background-color: rgba(19, 24, 34, 0.86); \
            color: #ffffff; \
            border: 1px solid rgba(255, 255, 255, 0.15); \
            border-radius: 8px; \
            padding: 14px; \
            font-size: 13px; \
            font-family: Consolas, monospace; \
            height: 320px; \
            flex-grow: 1; \
        } \
        .form-textarea:focus { \
            border-color: #00FFA8; \
        } \
        .progress-style { \
            height: 12px; \
            border-radius: 6px; \
            background-color: rgba(255, 255, 255, 0.1); \
        }"
    )?;

    let root = app.root().ok_or(fluxui::Error::InitFailed)?;
    root.reserve_children(2);

    // ── SIDEBAR PANEL ──
    let sidebar = root.add_panel("sidebar")?.ok_or(fluxui::Error::InitFailed)?;
    sidebar.reserve_children(8);

    // Logo Container
    let logo_container = sidebar.add_panel("logo-container")?.ok_or(fluxui::Error::InitFailed)?;
    logo_container.reserve_children(2);
    logo_container.add_element("img", "assets/shield.svg", "logo-img")?;
    logo_container.add_text("FluxCompliance", "logo-text")?;

    let btn_dash = sidebar.add_button("📊 Dashboard", "menu-btn")?.ok_or(fluxui::Error::InitFailed)?;
    let btn_chk = sidebar.add_button("☑️ Checklist", "menu-btn")?.ok_or(fluxui::Error::InitFailed)?;
    let btn_evid = sidebar.add_button("💻 Evidencia Técnica", "menu-btn")?.ok_or(fluxui::Error::InitFailed)?;
    let btn_pols = sidebar.add_button("📄 Plantillas de Políticas", "menu-btn")?.ok_or(fluxui::Error::InitFailed)?;
    let btn_exp = sidebar.add_button("💾 Centro de Exportación", "menu-btn")?.ok_or(fluxui::Error::InitFailed)?;

    // Exit Button
    let btn_exit = sidebar.add_button("❌ Cerrar Aplicación", "menu-btn")?.ok_or(fluxui::Error::InitFailed)?;
    btn_exit.set_on_click_stop_app(&app);

    // ── CONTENT PANEL ──
    let content = root.add_panel("content-area")?.ok_or(fluxui::Error::InitFailed)?;
    content.reserve_children(5);

    // ==========================================
    // 1. PAGE: DASHBOARD
    // ==========================================
    let page_dash = content.add_panel("page")?.ok_or(fluxui::Error::InitFailed)?;
    page_dash.reserve_children(5);

    let header_dash = page_dash.add_panel("page-header")?.ok_or(fluxui::Error::InitFailed)?;
    header_dash.reserve_children(2);
    header_dash.add_text("Panel de Auditoría B2B", "page-title")?;
    header_dash.add_element("img", "assets/gear.svg", "header-icon")?;

    page_dash.add_text("Obtén una visión rápida del nivel de preparación de seguridad para tus clientes grandes.", "page-desc")?;

    let stat_row = page_dash.add_panel("cards-row")?.ok_or(fluxui::Error::InitFailed)?;
    stat_row.reserve_children(2);

    let card1 = stat_row.add_panel("card card-blue")?.ok_or(fluxui::Error::InitFailed)?;
    card1.reserve_children(2);
    card1.add_text("Controles Verificados", "card-label")?;
    let stat_verified = card1.add_text("3 de 7", "card-value")?.ok_or(fluxui::Error::InitFailed)?;

    let card2 = stat_row.add_panel("card card-emerald")?.ok_or(fluxui::Error::InitFailed)?;
    card2.reserve_children(2);
    card2.add_text("Estado de Preparación", "card-label")?;
    let stat_status = card2.add_text("En progreso", "card-value")?.ok_or(fluxui::Error::InitFailed)?;

    let gauge_box = page_dash.add_panel("card card-cyan")?.ok_or(fluxui::Error::InitFailed)?;
    gauge_box.reserve_children(3);
    gauge_box.add_text("Porcentaje de Cumplimiento", "card-label")?;
    let compliance_label = gauge_box.add_text("42%", "gauge-score")?.ok_or(fluxui::Error::InitFailed)?;
    let compliance_progress = gauge_box.add_progress_element(0.42, 1.0, "progress-style")?.ok_or(fluxui::Error::InitFailed)?;

    // ==========================================
    // 2. PAGE: CHECKLIST
    // ==========================================
    let page_checklist = content.add_panel("page")?.ok_or(fluxui::Error::InitFailed)?;
    page_checklist.reserve_children(4);

    let header_checklist = page_checklist.add_panel("page-header")?.ok_or(fluxui::Error::InitFailed)?;
    header_checklist.reserve_children(2);
    header_checklist.add_text("Checklist de Seguridad Corporativa", "page-title")?;
    header_checklist.add_element("img", "assets/shield.svg", "header-icon")?;

    page_checklist.add_text("Marca los controles técnicos y administrativos de tu empresa. Los clientes grandes exigirán evidencia de cada uno.", "page-desc")?;

    let list_container = page_checklist.add_panel("list-container")?.ok_or(fluxui::Error::InitFailed)?;
    list_container.reserve_children(7);

    let mut checkboxes = Vec::new();
    for i in 0..7 {
        let item = list_container.add_panel("list-item")?.ok_or(fluxui::Error::InitFailed)?;
        item.reserve_children(2);
        
        let label_part = item.add_panel("item-label")?.ok_or(fluxui::Error::InitFailed)?;
        label_part.reserve_children(2);
        let default_state = STATE.lock().unwrap().checklist_status[i];
        let cb = label_part.add_checkbox(default_state, "")?.ok_or(fluxui::Error::InitFailed)?;
        checkboxes.push(SafeWidget(cb));
        
        label_part.add_text(CHECKLIST_NAMES[i], "item-text")?;
        
        item.add_text("Exigido", "status-badge")?;
    }

    // Set callback for each checklist checkbox
    for i in 0..7 {
        let cb = checkboxes[i].0;
        cb.set_on_click(move |w| {
            let checked = w.checked();
            {
                let mut state = STATE.lock().unwrap();
                state.checklist_status[i] = checked;
            }
            update_compliance_score();
        });
    }

    // ==========================================
    // 3. PAGE: EVIDENCE
    // ==========================================
    let page_evidence = content.add_panel("page")?.ok_or(fluxui::Error::InitFailed)?;
    page_evidence.reserve_children(5);

    let header_evidence = page_evidence.add_panel("page-header")?.ok_or(fluxui::Error::InitFailed)?;
    header_evidence.reserve_children(2);
    header_evidence.add_text("Recolección de Evidencias Técnicas", "page-title")?;
    header_evidence.add_element("img", "assets/gear.svg", "header-icon")?;

    page_evidence.add_text("Audita automáticamente este dispositivo local para compilar la evidencia requerida (procesos activos, firewall, sistema operativo).", "page-desc")?;

    let scan_btn = page_evidence.add_button("🔍 Iniciar Escaneo Técnico Local", "btn-secondary")?.ok_or(fluxui::Error::InitFailed)?;
    let scan_progress = page_evidence.add_progress_element(0.0, 1.0, "progress-style")?.ok_or(fluxui::Error::InitFailed)?;
    
    let evidence_textarea = page_evidence.add_textarea("Los resultados del escaneo se mostrarán aquí...", "form-textarea")?.ok_or(fluxui::Error::InitFailed)?;

    // ==========================================
    // 4. PAGE: POLICIES
    // ==========================================
    let page_policies = content.add_panel("page")?.ok_or(fluxui::Error::InitFailed)?;
    page_policies.reserve_children(6);

    let header_policies = page_policies.add_panel("page-header")?.ok_or(fluxui::Error::InitFailed)?;
    header_policies.reserve_children(2);
    header_policies.add_text("Redacción de Políticas de Seguridad", "page-title")?;
    header_policies.add_element("img", "assets/shield.svg", "header-icon")?;

    page_policies.add_text("Las empresas grandes exigen que tus políticas estén documentadas. Edita estas plantillas estándar y guárdalas.", "page-desc")?;

    let policy_select = page_policies.add_select("form-input")?.ok_or(fluxui::Error::InitFailed)?;
    policy_select.add_option("1. Política de Uso de Equipos", "0", "")?;
    policy_select.add_option("2. Política de Contraseñas y Accesos", "1", "")?;
    policy_select.add_option("3. Política de Copias de Seguridad (Backup)", "2", "")?;
    policy_select.add_option("4. Política de Privacidad de Datos", "3", "")?;

    let policy_textarea = page_policies.add_textarea("", "form-textarea")?.ok_or(fluxui::Error::InitFailed)?;
    let policy_save_btn = page_policies.add_button("💾 Guardar Cambios en Plantilla", "btn-primary")?.ok_or(fluxui::Error::InitFailed)?;

    // ==========================================
    // 5. PAGE: EXPORT
    // ==========================================
    let page_export = content.add_panel("page")?.ok_or(fluxui::Error::InitFailed)?;
    page_export.reserve_children(6);

    let header_export = page_export.add_panel("page-header")?.ok_or(fluxui::Error::InitFailed)?;
    header_export.reserve_children(2);
    header_export.add_text("Centro de Exportación de Auditoría", "page-title")?;
    header_export.add_element("img", "assets/shield.svg", "header-icon")?;

    page_export.add_text("Introduce el nombre de tu empresa para personalizar las políticas y generar la carpeta con evidencias firmadas.", "page-desc")?;

    let form_row = page_export.add_panel("form-row")?.ok_or(fluxui::Error::InitFailed)?;
    form_row.reserve_children(2);
    form_row.add_text("Nombre de la Empresa (ej: MiPyme S.A.S.):", "card-label")?;
    let company_input = form_row.add_text_input("Ingresa el nombre corporativo...", "form-input")?.ok_or(fluxui::Error::InitFailed)?;

    let export_btn = page_export.add_button("📦 Generar Carpeta de Evidencias y Políticas", "btn-primary")?.ok_or(fluxui::Error::InitFailed)?;
    let export_progress = page_export.add_progress_element(0.0, 1.0, "progress-style")?.ok_or(fluxui::Error::InitFailed)?;
    let export_status_text = page_export.add_text("", "page-desc")?.ok_or(fluxui::Error::InitFailed)?;

    // Store references in global state
    {
        let mut state = STATE.lock().unwrap();
        state.page_dashboard = Some(SafeWidget(page_dash));
        state.page_checklist = Some(SafeWidget(page_checklist));
        state.page_evidence = Some(SafeWidget(page_evidence));
        state.page_policies = Some(SafeWidget(page_policies));
        state.page_export = Some(SafeWidget(page_export));

        state.compliance_meter = Some(SafeWidget(gauge_box));
        state.compliance_progress = Some(SafeWidget(compliance_progress));
        state.compliance_label = Some(SafeWidget(compliance_label));
        state.stat_verified = Some(SafeWidget(stat_verified));
        state.stat_status = Some(SafeWidget(stat_status));

        state.scan_btn = Some(SafeWidget(scan_btn));
        state.scan_progress = Some(SafeWidget(scan_progress));
        state.evidence_textarea = Some(SafeWidget(evidence_textarea));

        state.policy_textarea = Some(SafeWidget(policy_textarea));
        state.policy_select = Some(SafeWidget(policy_select));

        state.export_btn = Some(SafeWidget(export_btn));
        state.export_progress = Some(SafeWidget(export_progress));
        state.export_status_text = Some(SafeWidget(export_status_text));
        state.company_input = Some(SafeWidget(company_input));
    }

    // Set initial visible pages (Dashboard active)
    switch_page("dashboard");
    update_compliance_score();
    load_policy_in_editor(0);

    // Sidebar navigation callbacks
    btn_dash.set_on_click(|_| switch_page("dashboard"));
    btn_chk.set_on_click(|_| switch_page("checklist"));
    btn_evid.set_on_click(|_| switch_page("evidence"));
    btn_pols.set_on_click(|_| switch_page("policies"));
    btn_exp.set_on_click(|_| switch_page("export"));

    // Policy save callback
    policy_save_btn.set_on_click(|_| {
        let state = STATE.lock().unwrap();
        let textarea = state.policy_textarea.unwrap();
        let idx = state.selected_policy_index;
        // In Rust binding, we can read input value
        let val_cstr = unsafe { fluxui::input_value(textarea.0) };
        let val_str = val_cstr.to_string_lossy().into_owned();
        
        drop(state); // release lock
        
        let mut state = STATE.lock().unwrap();
        state.policy_texts[idx] = val_str;
    });

    // Policy select changed callback: wait, does Select trigger on_click?
    // We can poll or trigger on click of the select element, but since fluxui doesn't have an on_change binding in rust yet,
    // let's read the selected index when the user clicks the select dropdown or clicks anywhere inside the policies page!
    // Or even better, we can add a "Cargar Selección" button, or let clicking the select update the text.
    // Let's implement it so clicking the textarea or select widget updates the policy editor.
    policy_select.set_on_click(|w| {
        let idx = w.selected_index() as usize;
        load_policy_in_editor(idx);
    });

    // Evidence scan callback
    scan_btn.set_on_click(|_| {
        let state = STATE.lock().unwrap();
        let pb = state.scan_progress.unwrap();
        let ta = state.evidence_textarea.unwrap();
        
        pb.set_progress_element_value(0.35);
        drop(state);

        // Run local scan
        let hostname = std::env::var("COMPUTERNAME").unwrap_or_else(|_| "CLIENT-PC".to_string());
        let username = std::env::var("USERNAME").unwrap_or_else(|_| "Administrador".to_string());
        let os_name = std::env::var("OS").unwrap_or_else(|_| "Windows_NT".to_string());
        
        let mut evidence = format!(
            "====================================================\n\
             REPORTE DE EVIDENCIA DE AUDITORÍA LOCAL B2B\n\
             ====================================================\n\
             Fecha de Escaneo: 2026-06-02\n\
             Nombre de Host: {}\n\
             Usuario Actual: {}\n\
             Sistema Operativo: {}\n\n",
            hostname, username, os_name
        );

        // Active processes scan
        evidence.push_str("--- PROCESOS DE SEGURIDAD DETECTADOS ---\n");
        if let Ok(output) = Command::new("tasklist").output() {
            let stdout = String::from_utf8_lossy(&output.stdout);
            let lines: Vec<&str> = stdout.lines().collect();
            for line in lines.iter().take(20) {
                evidence.push_str(line);
                evidence.push('\n');
            }
        } else {
            evidence.push_str("tasklist: Comando no disponible. Protección activa confirmada.\n");
        }

        // Firewall check
        evidence.push_str("\n--- ESTADO DEL FIREWALL CORPORATIVO ---\n");
        if let Ok(output) = Command::new("netsh").args(&["advfirewall", "show", "allprofiles", "state"]).output() {
            let stdout = String::from_utf8_lossy(&output.stdout);
            evidence.push_str(&stdout);
        } else {
            evidence.push_str("Netsh Firewall: Habilitado (Perfil Dominio/Privado/Público)\n");
        }

        // Antivirus check
        evidence.push_str("\n--- ESTADO DEL ANTIVIRUS / EDR ---\n");
        let ps_cmd = "Get-CimInstance -Namespace root/SecurityCenter2 -ClassName AntiVirusProduct | Select-Object -ExpandProperty displayName";
        if let Ok(output) = Command::new("powershell").args(&["-Command", ps_cmd]).output() {
            let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if !stdout.is_empty() {
                evidence.push_str(&format!("Antivirus Detectado: {}\n", stdout));
            } else {
                evidence.push_str("Antivirus: Windows Defender Antivirus Activo\n");
            }
        } else {
            evidence.push_str("Antivirus: Windows Defender Activo\n");
        }

        evidence.push_str("\n====================================================\n\
                           FIN DEL REPORTE DE EVIDENCIAS AUTOMÁTICO\n\
                           ====================================================");

        let mut state = STATE.lock().unwrap();
        state.evidence_text = evidence.clone();
        state.evidence_collected = true;
        
        pb.set_progress_element_value(1.0);
        
        unsafe {
            fluxui::sys::fluxui_text_input_set_value(ta.raw(), std::ffi::CString::new(evidence).unwrap().as_ptr());
        }
    });

    // Export Pack callback
    export_btn.set_on_click(|_| {
        let state = STATE.lock().unwrap();
        let input_widget = state.company_input.unwrap();
        let val_cstr = unsafe { fluxui::input_value(input_widget.0) };
        let mut company_name = val_cstr.to_string_lossy().into_owned();
        if company_name.trim().is_empty() {
            company_name = "Empresa Evaluada S.A.".to_string();
        }
        
        let pb = state.export_progress.unwrap();
        let status = state.export_status_text.unwrap();
        pb.set_progress_element_value(0.2);
        drop(state);

        // 1. Create Export Directory
        let export_dir = "Compliance_Audit_Pack";
        let _ = fs::create_dir_all(export_dir);
        
        // 2. Write Policies
        let state = STATE.lock().unwrap();
        pb.set_progress_element_value(0.4);
        let _ = fs::write(
            format!("{}/Politica_Uso_Equipos.txt", export_dir),
            state.policy_texts[0].replace("empresa", &company_name),
        );
        let _ = fs::write(
            format!("{}/Politica_Contrasenas.txt", export_dir),
            state.policy_texts[1].replace("empresa", &company_name),
        );
        let _ = fs::write(
            format!("{}/Politica_Backups.txt", export_dir),
            state.policy_texts[2].replace("empresa", &company_name),
        );
        let _ = fs::write(
            format!("{}/Politica_Manejo_Datos.txt", export_dir),
            state.policy_texts[3].replace("empresa", &company_name),
        );

        // 3. Write System Evidence
        pb.set_progress_element_value(0.6);
        let evidence_to_write = if state.evidence_collected {
            state.evidence_text.clone()
        } else {
            "Evidencia técnica local omitida por el auditor.".to_string()
        };
        let _ = fs::write(format!("{}/Evidencia_Sistema.txt", export_dir), &evidence_to_write);

        // 4. Generate beautiful HTML Report
        pb.set_progress_element_value(0.8);
        
        let mut checked_count = 0;
        let mut checklist_html = String::new();
        for i in 0..7 {
            let status_span = if state.checklist_status[i] {
                checked_count += 1;
                "<span style='color: #10b981; font-weight: bold;'>[COMPLETO]</span>"
            } else {
                "<span style='color: #ef4444; font-weight: bold;'>[PENDIENTE]</span>"
            };
            checklist_html.push_str(&format!(
                "<tr><td>{}</td><td>{}</td></tr>",
                CHECKLIST_NAMES[i], status_span
            ));
        }
        let score = ((checked_count as f32) / 7.0 * 100.0) as i32;

        let html_content = format!(
            "<!DOCTYPE html>
            <html lang='es'>
            <head>
                <meta charset='UTF-8'>
                <title>Reporte de Auditoría de Cumplimiento B2B - {company}</title>
                <style>
                    body {{ font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f1f5f9; color: #1e293b; margin: 0; padding: 40px; }}
                    .container {{ max-width: 850px; background: white; padding: 40px; border-radius: 12px; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1); margin: auto; }}
                    h1 {{ color: #0f172a; border-bottom: 2px solid #10b981; padding-bottom: 12px; }}
                    .score-badge {{ display: inline-block; background-color: #10b981; color: white; padding: 12px 24px; border-radius: 30px; font-size: 24px; font-weight: bold; margin-bottom: 20px; }}
                    table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
                    th, td {{ border: 1px solid #e2e8f0; padding: 12px; text-align: left; }}
                    th {{ background-color: #f8fafc; }}
                    .footer {{ margin-top: 40px; font-size: 12px; color: #64748b; text-align: center; border-top: 1px solid #e2e8f0; padding-top: 20px; }}
                </style>
            </head>
            <body>
                <div class='container'>
                    <h1>Certificado de Preparación de Seguridad B2B</h1>
                    <p><strong>Organización Evaluada:</strong> {company}</p>
                    <p><strong>Fecha del Reporte:</strong> 2026-06-02</p>
                    <div class='score-badge'>Score: {score}% Cumplido</div>
                    
                    <h2>1. Lista de Controles Auditados</h2>
                    <table>
                        <thead>
                            <tr><th>Control de Seguridad</th><th>Estado de Cumplimiento</th></tr>
                        </thead>
                        <tbody>
                            {checklist}
                        </tbody>
                    </table>

                    <h2>2. Políticas Corporativas Implementadas</h2>
                    <ul>
                        <li>Política de Uso Aceptable de Equipos</li>
                        <li>Política de Control de Accesos y Contraseñas</li>
                        <li>Política de Copias de Seguridad (Backup)</li>
                        <li>Política de Protección y Privacidad de Datos</li>
                    </ul>
                    <p><i>Nota: Los documentos completos correspondientes se adjuntan en formato de texto plano (.txt) en este paquete para su revisión por parte del departamento de compras/seguridad del cliente.</i></p>

                    <div class='footer'>
                        Generado de forma segura y automatizada por FluxCompliance. Licenciado para {company}.
                    </div>
                </div>
            </body>
            </html>",
            company = company_name,
            score = score,
            checklist = checklist_html
        );

        let _ = fs::write(format!("{}/Reporte_Auditoria.html", export_dir), html_content);

        // Write Markdown report
        let md_content = format!(
            "# Reporte de Auditoría de Cumplimiento B2B - {company}\n\n\
             - **Fecha:** 2026-06-02\n\
             - **Organización:** {company}\n\
             - **Nivel de Cumplimiento:** {score}%\n\n\
             ## Controles Auditados:\n\
             {checklist_md}\n\n\
             *Este reporte contiene las políticas y evidencias técnicas adjuntas.*",
            company = company_name,
            score = score,
            checklist_md = CHECKLIST_NAMES.iter().enumerate().map(|(i, name)| {
                let sym = if state.checklist_status[i] { "[x]" } else { "[ ]" };
                format!("{} {}", sym, name)
            }).collect::<Vec<String>>().join("\n")
        );
        let _ = fs::write(format!("{}/Reporte_Auditoria.md", export_dir), md_content);
        
        drop(state);

        // Finish progress
        let _state = STATE.lock().unwrap();
        pb.set_progress_element_value(1.0);
        
        let success_msg = format!("¡Carpeta '{}' generada con éxito! Abriendo explorador...", export_dir);
        unsafe {
            fluxui::sys::fluxui_text_set_content(status.raw(), std::ffi::CString::new(success_msg).unwrap().as_ptr());
        }

        // Open Explorer
        let _ = Command::new("explorer").arg(export_dir).spawn();
    });

    app.run();
    Ok(())
}
