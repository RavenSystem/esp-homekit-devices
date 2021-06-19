(
 (nil
  )
 (c-mode
  (indent-tabs-mode . nil)
  (c-file-style . "bsd")
  (c-basic-offset . 4)
  )
 (asm-mode
  (indent-tabs-mode . nil)
  ; this is basically a hack so asm-mode indents with spaces not tabs
  ; taken from http://stackoverflow.com/questions/2668563/emacs-indentation-in-asm-mode
  ; (moving to gas-mode may be a better choice)
  (tab-stop-list (quote (4 8 12 16 20 24 28 32 36 40 44 48 52 56 60 64 68 72 76 80 84 88 92 96 100 104 108 112 116 120)))
  (asm-comment-char . "#")
  )
 )

; IMPORTANT: If you want to write assembly and have indenting to not be infuriating,
; you probably also want this in your .emacs file:
;
; (add-hook 'asm-mode-hook '(lambda () (setq indent-line-function 'indent-relative)))
;
; This is not safe to set as a local variable.

