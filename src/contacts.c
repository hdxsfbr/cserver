#include "contacts.h"

enum { MAX_CONTACTS = 100 };

static Contact contacts[MAX_CONTACTS];
static int contact_count = 0;
static int next_id = 1;

static int contact_index_by_id(int id) {
    for (int i = 0; i < contact_count; i++) {
        if (contacts[i].id == id) {
            return i;
        }
    }
    return -1;
}

int contacts_count(void) {
    return contact_count;
}

const Contact *contacts_all(void) {
    return contacts;
}

int contacts_get(int id, Contact *out_contact) {
    int idx = contact_index_by_id(id);
    if (idx < 0) {
        return 0;
    }
    if (out_contact) {
        *out_contact = contacts[idx];
    }
    return 1;
}

int contacts_create(const Contact *input, Contact *created_contact) {
    if (contact_count >= MAX_CONTACTS) {
        return 0;
    }
    Contact contact = *input;
    contact.id = next_id++;
    contacts[contact_count++] = contact;
    if (created_contact) {
        *created_contact = contact;
    }
    return 1;
}

int contacts_update(int id, const Contact *input, Contact *updated_contact) {
    int idx = contact_index_by_id(id);
    if (idx < 0) {
        return 0;
    }
    Contact updated = *input;
    updated.id = id;
    contacts[idx] = updated;
    if (updated_contact) {
        *updated_contact = updated;
    }
    return 1;
}

int contacts_delete(int id) {
    int idx = contact_index_by_id(id);
    if (idx < 0) {
        return 0;
    }
    for (int i = idx; i < contact_count - 1; i++) {
        contacts[i] = contacts[i + 1];
    }
    contact_count--;
    return 1;
}
