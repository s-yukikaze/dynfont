if exists('g:loaded_dynfont')
  finish
endif
let g:loaded_dynfont = 1

command! -nargs=1 -complete=file DynamicFont :echo dynfont#load(<f-args>)

if has('iconv')"{{{
  let s:termencoding = &termencoding
  if s:termencoding == ''
    let s:termencoding = 'default'
  endif

  function! s:iconv(source, from, to)
    return iconv(a:source, a:from, a:to)
  endfunction
else
  function! s:iconv(source, from, to)
    return a:source
  endfunction
endif"}}}

let s:dynfont_enabled = 0
if has('win32')"{{{
  let s:so_path = expand("<sfile>:p:h") . "/dynfont.dll"
  let s:so_path = s:iconv(s:so_path, &encoding, s:termencoding)
  if filereadable(s:so_path)
    let s:dynfont_enabled = 1
  endif
endif"}}}

if s:dynfont_enabled != 0"{{{
  function! dynfont#load(fontpath)
    let fontpath = s:iconv(a:fontpath, &encoding, s:termencoding)
    let succeeded = libcall(s:so_path, "dynamic_font", fontpath)
    return succeeded
  endfunction
else
  function! dynfont#load(fontpath)
    return "false"
  endfunction
endif"}}}

" vim: foldmethod=marker
