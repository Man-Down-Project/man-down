use std::collections::HashSet;

#[derive(Debug, Clone)]
pub struct RfidSessionState {
    logged_in_tags: HashSet<String>,
}

impl RfidSessionState {
    pub fn new() -> Self {
        Self {
            logged_in_tags: HashSet::new(),
        }
    }

    pub fn handle_scan(&mut self, tag_id: &str) -> bool {
        if self.logged_in_tags.contains(tag_id) {
            self.logged_in_tags.remove(tag_id);
            false
        } else {
            self.logged_in_tags.insert(tag_id.to_string());
            true
        }
    }

    #[allow(dead_code)]
    pub fn is_logged_in(&self, tag_id: &str) -> bool {
        self.logged_in_tags.contains(tag_id)
    }
}
