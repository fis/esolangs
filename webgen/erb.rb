require 'erb'
require 'optparse'
require 'psych'

WELL_KNOWN_STYLES = {
  'bootstrap' => {
    'href' => '//maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css',
    'integrity' => 'sha384-Gn5384xqQ1aoWXA+058RXPxPg6fy4IWvTNh0E263XmFcJlSAwiGgFAW/dAiS6JXm',
    'crossorigin' => 'anonymous',
  }
}

WELL_KNOWN_SCRIPTS = {
  'bootstrap' => {
    'src' => '//maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js',
    'integrity' => 'sha384-JZR6Spejh4U02d8jOt6vLEHfe/JQGiRRSQQxSfFWpi1MquVdAyjUar5+76PVCmYl',
    'crossorigin' => 'anonymous',
  },
  'd3' => {
    'src' => '//cdnjs.cloudflare.com/ajax/libs/d3/4.13.0/d3.min.js',
    'integrity' => 'sha384-1EOYqz4UgZkewWm70NbT1JBUXSQpOIS2AaJy6/evZH+lXOrt9ITSJbFctNeyBoIJ',
    'crossorigin' => 'anonymous',
  },
  'jquery' => {
    'src' => '//cdnjs.cloudflare.com/ajax/libs/jquery/3.3.1/jquery.min.js',
    'integrity' => 'sha384-tsQFqpEReu7ZLhBV2VZlAu7zcOV+rXbYlF2cqB8txI/8aZajjp4Bqd+V6D5IgvKT',
    'crossorigin' => 'anonymous',
  },
  'mathjax' => {
    'src' => '//cdnjs.cloudflare.com/ajax/libs/mathjax/2.7.3/MathJax.js?config=TeX-AMS_HTML',
    'integrity' => 'sha384-ap4cGfAEIeLaf8QO+dKYPeksHk3N8bTpT5bWKtwCFQjItwQoNKjTsBUEJnxbM4vA',
    'crossorigin' => 'anonymous',
  },
  'popper' => {
    'src' => '//cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js',
    'integrity' => 'sha384-ApNbgh9B+Y1QKtv3Rn7W3mgPxhU9K/ScQsAP7hUibX39j7fakFPskvXusvfa0b4Q',
    'crossorigin' => 'anonymous',
  },
}

# TODO: model script dependencies and/or a priority level
# guard against double inclusion

def add_attributes(out, attrs, well_known_map)
  if attrs.include?(:name)
    wk = well_known_map[attrs[:name]]
    raise RuntimeError, "unknown name: #{attrs[:name]}" unless wk
    attrs = attrs.merge(wk)
  end
  attrs.each {|attr, val| out << ' %s="%s"' % [attr, val] if attr.is_a?(String)}
end

class Renderer
  def initialize(opts)
    @styles = opts[:styles].map.with_index {|s,i| { name: s, order: i }}
    @scripts = opts[:scripts].map.with_index {|s,i| { name: s, order: i }}
    @template_counters = { style: 100, script: 100 }
  end

  def set_order(item, kind)
    unless item.include?(:order)
      item[:order] = @template_counters[kind]
      @template_counters[kind] += 1
    end
    item
  end

  def load_file(file)
    template = Psych.load_file(file)
    raise RuntimeError, "#{file}: template name not specified" unless template.include? 'name'
    data, in_data, separators = '', false, 0
    File.open(file, 'r') do |f|
      while line = f.gets
        if in_data
          data << line
        else
          if line == "---\n"
            separators += 1
            in_data = true if separators == 2
          end
        end
      end
    end
    template[:erb] = ERB.new(data)
    instance_variable_set("@#{template['name']}".to_sym, template)
    template['styles'].to_a.each {|s| @styles.push(set_order(s, :style))}
    template['scripts'].to_a.each {|s| @scripts.push(set_order(s, :script))}
  end

  def render(template, args = {})
    b = binding
    b.local_variable_set(:args, args)
    template[:erb].result(b)
  end

  def styles
    tags = ''
    @styles.sort {|a,b| a[:order] <=> b[:order]}.each do |s|
      tags << '<link rel="stylesheet" type="text/css"'
      add_attributes(tags, s, WELL_KNOWN_STYLES)
      tags << '>'
    end
    tags
  end

  def scripts
    tags = ''
    @scripts.sort {|a,b| a[:order] <=> b[:order]}.each do |s|
      tags << '<script'
      add_attributes(tags, s, WELL_KNOWN_SCRIPTS)
      tags << '></script>'
    end
    tags
  end
end

options = {
  main: 'default',
  styles: [],
  scripts: [],
}

OptionParser.new do |opts|
  opts.banner = 'Usage: erb.rb [options] template ...'

  opts.on('--main=TEMPLATE', 'Main template name') do |m|
    options[:main] = m
  end
  opts.on('--out=FILE', 'Output to file instead of stdout') do |o|
    options[:out] = o
  end
  opts.on('--style=NAME', 'Include a well-known stylesheet even if not requested by template') do |s|
    options[:styles].push(s)
  end
  opts.on('--script=NAME', 'Include a well-known script even if not requested by template') do |s|
    options[:scripts].push(s)
  end
end.parse!

renderer = Renderer.new(options)
ARGV.each {|f| renderer.load_file(f)}

main = renderer.instance_variable_get("@#{options[:main]}".to_sym)
raise RuntimeError, "main template #{options[:main]} not found" unless main
output = renderer.render(main)

if options.include? :out
  File.open(options[:out], 'w') {|f| f.write(output)}
else
  STDOUT.write(output)
end
