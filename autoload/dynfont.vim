if exists('g:loaded_dynfont')
  finish
endif
let g:loaded_dynfont = 1

command! -nargs=1 -complete=file DynamicFont :echo dynfont#dynamic_font(<f-args>)
command! -nargs=0 EnableDirectWrite :call s:enable_directwrite('1')
command! -nargs=0 DisableDirectWrite :call s:enable_directwrite('0')

if has('iconv')
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
endif

let s:dynfont_enabled = 0
if has('win64')
  let s:so_name = "dynfont_win64.dll"
elseif has('win32')
  let s:so_name = "dynfont_win32.dll"
else
  let s:so_name = ""
endif

if s:so_name != ""
  let s:so_path = expand("<sfile>:p:h") . "/" . s:so_name
  let s:so_path = s:iconv(s:so_path, &encoding, s:termencoding)
  if filereadable(s:so_path)
    let s:dynfont_enabled = 1
  endif
endif

if s:dynfont_enabled != 0
  function! dynfont#load(fontpath)
    let fontpath = s:iconv(a:fontpath, &encoding, s:termencoding)
    let succeeded = libcall(s:so_path, "dynamic_font", fontpath)
    return succeeded
  endfunction

else
  function! dynfont#load(fontpath)
    return "false"
  endfunction
endif


