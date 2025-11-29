/**
 * File Manager UI
 */
var fileManager = {
  el: null,
  currentCategory: 'roms',
  pendingDeletePath: null,
  pendingRenamePath: null,

  // Category to directory mapping
  categoryDirs: {
    roms: '/roms',
    projects: '/projects',
    saves: '/saves',
    recent: null  // Special handling
  },

  // File extension icons
  fileIcons: {
    '.sfc': 'memory',
    '.smc': 'memory',
    '.srm': 'save',
    '.sav': 'save',
    '.yproj': 'folder',
    '.md': 'description',
    'default': 'insert_drive_file'
  },

  init: function() {
    this.el = document.getElementById('file-manager-modal');

    // Close on backdrop click
    this.el.addEventListener('click', (e) => {
      if (e.target === this.el) this.hide();
    });

    // Setup file upload handler
    var uploadInput = document.getElementById('file-upload-input');
    if (uploadInput) {
      uploadInput.addEventListener('change', (e) => {
        if (e.target.files && e.target.files[0]) {
          this.handleUpload(e.target.files[0]);
          e.target.value = ''; // Reset for re-upload
        }
      });
    }

    // Handle Enter key in rename input
    var renameInput = document.getElementById('rename-input');
    if (renameInput) {
      renameInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
          e.preventDefault();
          this.confirmRename();
        } else if (e.key === 'Escape') {
          this.cancelRename();
        }
      });
    }
  },

  show: function() {
    this.el.style.display = 'flex';
    this.refreshList();
    this.updateStorageInfo();
  },

  hide: function() {
    this.el.style.display = 'none';
  },

  switchCategory: function(category) {
    this.currentCategory = category;

    // Update tab buttons
    document.querySelectorAll('.file-tab').forEach(function(tab) {
      tab.classList.toggle('active', tab.dataset.category === category);
    });

    this.refreshList();
  },

  refreshList: function() {
    var listEl = document.getElementById('file-list');
    if (!listEl) return;

    // Check if filesystem is ready
    if (!FilesystemManager.ensureReady(false)) {
      listEl.innerHTML = '<div class="file-list-empty">Filesystem not ready. Please wait...</div>';
      return;
    }

    var files = [];

    if (this.currentCategory === 'recent') {
      // Get recent files
      var recent = FilesystemManager.getRecentFiles(20);
      files = recent.map(function(r) {
        var info = FilesystemManager.getFileInfo(r.filename);
        return {
          name: r.filename.split('/').pop(),
          path: r.filename,
          size: info ? info.size : 0,
          mtime: r.timestamp ? new Date(r.timestamp) : null,
          isDirectory: false
        };
      });
    } else {
      // Get files from directory
      var dir = this.categoryDirs[this.currentCategory];
      if (dir) {
        files = FilesystemManager.getDirectoryListing(dir);
      }
    }

    // Filter out directories and sort by name
    files = files.filter(f => !f.isDirectory);
    files.sort((a, b) => a.name.localeCompare(b.name));

    if (files.length === 0) {
      listEl.innerHTML = '<div class="file-list-empty">No files found</div>';
      return;
    }

    var self = this;
    var html = files.map(function(file) {
      var icon = self.getFileIcon(file.name);
      var size = self.formatSize(file.size);
      var date = file.mtime ? self.formatDate(file.mtime) : '-';

      return '<div class="file-item" data-path="' + self.escapeHtml(file.path) + '">' +
        '<div class="file-item-name">' +
          '<span class="material-symbols-outlined">' + icon + '</span>' +
          '<span>' + self.escapeHtml(file.name) + '</span>' +
        '</div>' +
        '<div class="file-item-size">' + size + '</div>' +
        '<div class="file-item-date">' + date + '</div>' +
        '<div class="file-item-actions">' +
          '<button class="file-action-btn" onclick="fileManager.renameFile(\'' + self.escapeHtml(file.path) + '\')" title="Rename">' +
            '<span class="material-symbols-outlined">edit</span>' +
          '</button>' +
          '<button class="file-action-btn" onclick="fileManager.downloadFile(\'' + self.escapeHtml(file.path) + '\')" title="Download">' +
            '<span class="material-symbols-outlined">download</span>' +
          '</button>' +
          '<button class="file-action-btn danger" onclick="fileManager.deleteFile(\'' + self.escapeHtml(file.path) + '\')" title="Delete">' +
            '<span class="material-symbols-outlined">delete</span>' +
          '</button>' +
        '</div>' +
      '</div>';
    }).join('');

    listEl.innerHTML = html;
  },

  updateStorageInfo: function() {
    FilesystemManager.getStorageUsage().then(function(info) {
      var bar = document.getElementById('storage-bar');
      var text = document.getElementById('storage-text');

      if (bar) {
        bar.style.width = info.percent + '%';
        bar.classList.remove('warning', 'critical');
        if (info.percent > 90) {
          bar.classList.add('critical');
        } else if (info.percent > 70) {
          bar.classList.add('warning');
        }
      }

      if (text) {
        var usedMB = (info.usage / (1024 * 1024)).toFixed(1);
        var totalMB = (info.quota / (1024 * 1024)).toFixed(0);
        text.textContent = info.percent + '% (' + usedMB + 'MB / ' + totalMB + 'MB)';
      }
    });
  },

  getFileIcon: function(filename) {
    var ext = filename.toLowerCase().match(/\.[^.]+$/);
    ext = ext ? ext[0] : '';
    return this.fileIcons[ext] || this.fileIcons['default'];
  },

  formatSize: function(bytes) {
    if (!bytes) return '0 B';
    var k = 1024;
    var sizes = ['B', 'KB', 'MB', 'GB'];
    var i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  },

  formatDate: function(date) {
    if (!date || !(date instanceof Date)) return '-';
    var year = date.getFullYear();
    var month = String(date.getMonth() + 1).padStart(2, '0');
    var day = String(date.getDate()).padStart(2, '0');
    return year + '-' + month + '-' + day;
  },

  escapeHtml: function(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML.replace(/'/g, "\\'");
  },

  // Delete file with confirmation
  deleteFile: function(path) {
    this.pendingDeletePath = path;
    var filename = path.split('/').pop();
    var msg = document.getElementById('delete-confirm-message');
    if (msg) {
      msg.textContent = 'Are you sure you want to delete "' + filename + '"? This action cannot be undone.';
    }
    document.getElementById('delete-confirm-dialog').style.display = 'flex';
  },

  confirmDelete: function() {
    if (this.pendingDeletePath) {
      var success = FilesystemManager.deleteFile(this.pendingDeletePath);
      if (success) {
        console.log('File deleted:', this.pendingDeletePath);
      } else {
        alert('Failed to delete file');
      }
      this.pendingDeletePath = null;
    }
    document.getElementById('delete-confirm-dialog').style.display = 'none';
    this.refreshList();
    this.updateStorageInfo();
  },

  cancelDelete: function() {
    this.pendingDeletePath = null;
    document.getElementById('delete-confirm-dialog').style.display = 'none';
  },

  // Rename file with dialog
  renameFile: function(path) {
    this.pendingRenamePath = path;
    var filename = path.split('/').pop();
    var input = document.getElementById('rename-input');
    if (input) {
      input.value = filename;
      input.select();
    }
    document.getElementById('rename-dialog').style.display = 'flex';
    if (input) input.focus();
  },

  confirmRename: function() {
    var input = document.getElementById('rename-input');
    var newName = input ? input.value.trim() : '';

    if (!newName || !this.pendingRenamePath) {
      alert('Please enter a valid filename');
      return;
    }

    // Build new path
    var oldPath = this.pendingRenamePath;
    var dir = oldPath.substring(0, oldPath.lastIndexOf('/'));
    var newPath = dir + '/' + newName;

    if (oldPath !== newPath) {
      var success = FilesystemManager.renameFile(oldPath, newPath);
      if (success) {
        console.log('File renamed:', oldPath, '->', newPath);
      } else {
        alert('Failed to rename file');
      }
    }

    this.pendingRenamePath = null;
    document.getElementById('rename-dialog').style.display = 'none';
    this.refreshList();
  },

  cancelRename: function() {
    this.pendingRenamePath = null;
    document.getElementById('rename-dialog').style.display = 'none';
  },

  // Download file
  downloadFile: function(path) {
    FilesystemManager.downloadFile(path);
  },

  // Handle file upload
  handleUpload: function(file) {
    if (!FilesystemManager.ensureReady()) return;

    var self = this;
    var reader = new FileReader();

    reader.onload = function(e) {
      var data = new Uint8Array(e.target.result);
      var dir = self.categoryDirs[self.currentCategory] || '/roms';
      var filename = dir + '/' + file.name;

      try {
        // Ensure directory exists
        try { FS.mkdir(dir); } catch(mkErr) {}

        FS.writeFile(filename, data);
        console.log('Uploaded file:', filename, data.length, 'bytes');

        // Sync to IndexedDB
        FilesystemManager.syncAll();

        self.refreshList();
        self.updateStorageInfo();
      } catch (err) {
        console.error('Upload error:', err);
        alert('Failed to upload file: ' + err.message);
      }
    };

    reader.onerror = function() {
      alert('Failed to read file');
    };

    reader.readAsArrayBuffer(file);
  }
};

// Initialize file manager on load
document.addEventListener('DOMContentLoaded', function() {
  fileManager.init();
});
